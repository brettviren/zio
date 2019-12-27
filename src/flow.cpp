#include "zio/flow.hpp"

zio::flow::Flow::Flow(zio::portptr_t port, zio::Message bot)
    : m_port(port)
    , m_credits(0)
    , m_total_credits(0)
    , m_sender(true)
{
    m_port->send(bot);

    bool ok = m_port->recv(bot);
    assert(ok);
    auto lobj = bot.label_object();

    m_total_credits = lobj["credits"];

    // lobj is from the point of view of the other end
    if (lobj["direction"] = "extract") { 
        // they want extraction so we are reciever
        m_sender = false;
        // recver starts flush with credits
        m_credits = m_total_credits;
    }
    else {
        // they want injection so we are sender
        m_sender = true;
        // sender must wait for some PAY
        m_credits = 0;          
    }

}


// try to receive a PAY message.  Return number of credits
// received.  0 indicates timeout.  -1 indicates EOT
// message was received instead.  -2 indicates unexpected
// message.

static
int slurp_credit(zio::portptr_t port, int timeout)
{
    zio::Message msg;
    bool ok = port->recv(msg, timeout);
    if (!ok) {              // timeout
        return 0;
    }
    auto obj = msg.label_object();
    std::string flowtype = obj["flow"];
    if (flowtype == "PAY") {
        int credit = obj["credit"];
        return credit;
    }
    if (flowtype == "EOT") {
        return -1;
    }
    return -2;
}


bool zio::flow::Flow::put(zio::Message dat)
{
    if (m_credits < m_total_credits) {
        // quick try to get any PAY already sitting in buffers
        int c = slurp_credit(m_port, 0);
        if (c < 0) {
            return false;
        }
        m_credits += c;
    }
    if (m_credits == 0) {
        // not credits, we really have to wait until we get some PAY
        int c = slurp_credit(m_port, -1);
        if (c < 0) {
            return false;
        }
        assert (c>0);
        m_credits = c;
    }
    m_port->send(dat);
    --m_credits;
    return true;
}


bool zio::flow::Flow::get(zio::Message& dat, int timeout)
{
    zio::Message msg;
    if (m_credits) {
        zio::json o{{"flow","PAY"},{"credit",m_credits}};
        msg.set_label(o.dump());
        m_credits=0;
        m_port->send(msg);
    }
    bool ok = m_port->recv(msg, timeout);
    if (!ok) { return false; }
    auto lobj = msg.label_object();
    if (lobj["flow"] != "DAT") {
        return false;
    }
    dat = msg;
    return true;
}

bool zio::flow::Flow::eot(Message& msg, int timeout)
{
    m_port->send(msg);
    while (true) {
        bool ok = m_port->recv(msg, timeout);
        if (!ok) {              // timeout
            return false;
        }
        auto lobj = msg.label_object();
        if (lobj["flow"] != "EOT") {
            return true;
        }
    }
}











zio::flow::Sender::Sender(portptr_t port, int routing_id)
    : m_port(port)
    , m_rid(routing_id)
    , m_flowmsg({{zio::level::undefined,"FLOW",""},{0,0,0}})
{
}

void zio::flow::Sender::bot(flow::Direction fd, int credits,
                            const zio::json& extra,
                            const payload_t& payload)
{
    const std::vector<std::string> dir2nam = {
        "undefined", "extract", "inject" };
    auto obj = extra;
    obj["flow"] = "BOT";
    obj["direction"] = dir2nam[fd];
    obj["credits"] = credits;
    send(obj, payload);
}
void zio::flow::Sender::eot(const zio::json& extra,
                            const payload_t& payload)
{
    auto obj = extra;
    obj["flow"] = "EOT";
    send(obj, payload);
}
void zio::flow::Sender::pay(int credits,
                            const zio::json& extra,
                            const payload_t& payload)
{
    auto obj = extra;
    obj["flow"] = "PAY";
    obj["credits"] = credits;
    send(obj, payload);
}
void zio::flow::Sender::dat(const zio::json& extra,
                            const payload_t& payload)
{
    auto obj = extra;
    obj["flow"] = "DAT";
    send(obj, payload);
}


void zio::flow::Sender::send(const zio::json& flowobj,
                             const payload_t& payload)
{
    // this should probably be subsumed into zio::Socket
    if (m_rid and m_port->socket().type() == ZMQ_SERVER) {
        zsock_set_routing_id(m_port->socket().zsock(), m_rid);
    }
    m_flowmsg.set_label(flowobj.dump());
    m_flowmsg.payload().clear();
    m_flowmsg.payload().push_back(payload);
    m_port->send(m_flowmsg);
}



// Server

zio::flow::Server::Server(portptr_t port, int max_credits)
    : m_port(port)
    , m_max_credits(max_credits)
{
    assert(m_port->socket().type() == ZMQ_SERVER);
}

zio::flow::endpoint_t* zio::flow::Server::recv(int timeout)
{
    Message msg;
    bool ok = m_port->recv(msg, timeout);
    if (!ok) { return nullptr; }
    if (msg.format() != "FLOW") { return nullptr; }

    auto flowobj = zio::json::parse(msg.label());

    // this should probably be subsumed into zio::Socket
    int cid = zsock_routing_id(m_port->socket().zsock());

    zio::flow::endpoint_t* cl = endpoint(cid);
    if (!cl) {
        if (flowobj["flow"] != "BOT") {
            return nullptr;
        }
        std::string  dir;
        if (flowobj["direction"].is_string()) {
            dir = flowobj["direction"];
        }
        else {
            return nullptr;
        }
        if (!(dir == "extract" or dir == "inject")) {
            return nullptr;
        }
        int max_credits = m_max_credits;
        if (flowobj["credits"].is_number()) {
            int want = flowobj["credits"];
            max_credits = std::min(want, max_credits);
        }
        int credits = max_credits; // inject, server starts with full credit
        zio::flow::Direction idir = zio::flow::inject;
        if (dir == "extract") {
            idir = zio::flow::extract;
            credits = 0;        // server starts with no credit
        }


        m_endpoints.emplace(std::make_pair(cid, endpoint_t{cid, credits, max_credits,
                        idir, Sender(m_port, cid), msg}));
        return endpoint(cid);
    }
    cl->flowmsg = msg;
    return cl;
}

zio::flow::endpoint_t* zio::flow::Server::endpoint(int cid)
{
    auto it = m_endpoints.find(cid);
    if (it == m_endpoints.end()) {
        return nullptr;
    }
    return &it->second;
}
void zio::flow::Server::destroy(zio::flow::endpoint_t** clptr)
{
    if (!clptr) { return; }
    if (!*clptr) { return; }
    int cid = (**clptr).id;
    zio::flow::endpoint_t* mine = endpoint(cid);
    if (mine) {
        m_endpoints.erase(cid);
    }
    else {
        delete *clptr;
        *clptr = nullptr;
    }
}



/// client

zio::flow::Client::Client(portptr_t port,
                          zio::flow::Direction direction,
                          int suggested_credits)
    : m_port(port)
    , m_endpoint{0, 0, 0, zio::flow::undefined, Sender(port)}
{
    m_endpoint.send.bot(direction, suggested_credits);
}

zio::flow::endpoint_t* zio::flow::Client::recv(int timeout)
{
    Message msg;
    bool ok = m_port->recv(msg, timeout);
    if (!ok) { return nullptr; }
    if (msg.format() != "FLOW") { return nullptr; }

    auto flowobj = zio::json::parse(msg.label());

    // intercept BOT reply
    if (flowobj["flow"] = "BOT") {
        std::string  dir;
        if (flowobj["direction"].is_string()) {
            dir = flowobj["direction"];
        }
        else {
            return nullptr;
        }
        if (!(dir == "extract" or dir == "inject")) {
            return nullptr;
        }
        m_endpoint.credits = 0;
        m_endpoint.total_credits = flowobj["credits"];
        zio::flow::Direction idir = zio::flow::inject;
        if (dir == "extract") {
            idir = zio::flow::extract;
            m_endpoint.credits = m_endpoint.total_credits;
        }
        m_endpoint.direction = idir;
    }
    m_endpoint.flowmsg = msg;
    return &m_endpoint;
}
