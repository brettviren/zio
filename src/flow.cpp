
#include "zio/flow.hpp"

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

zio::flow::Server::client_t* zio::flow::Server::recv(int timeout)
{
    Message msg;
    bool ok = m_port->recv(msg, timeout);
    if (!ok) { return nullptr; }
    if (msg.format() != "FLOW") { return nullptr; }

    auto flowobj = zio::json::parse(msg.label());

    // this should probably be subsumed into zio::Socket
    int cid = zsock_routing_id(m_port->socket().zsock());

    client_t* cl = client(cid);
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


        m_clients.emplace(std::make_pair(cid, client_t{cid, credits, max_credits,
                        idir, Sender(m_port, cid), msg}));
        return client(cid);
    }
    cl->flowmsg = msg;
    return cl;
}

zio::flow::Server::client_t* zio::flow::Server::client(int cid)
{
    auto it = m_clients.find(cid);
    if (it == m_clients.end()) {
        return nullptr;
    }
    return &it->second;
}
void zio::flow::Server::destroy(zio::flow::Server::client_t** clptr)
{
    if (!clptr) { return; }
    if (!*clptr) { return; }
    int cid = (**clptr).id;
    client_t* mine = client(cid);
    if (mine) {
        m_clients.erase(cid);
    }
    else {
        delete *clptr;
        *clptr = nullptr;
    }
}



/// client

zio::flow::Client::Client(portptr_t port)
    : m_port(port)
    , m_server{0, 0, 0, zio::flow::undefined, Sender(port)}
{
}

zio::flow::Client::server_t* zio::flow::Client::recv(int timeout)
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
        m_server.credits = 0;
        m_server.total_credits = flowobj["credits"];
        zio::flow::Direction idir = zio::flow::inject;
        if (dir == "extract") {
            idir = zio::flow::extract;
            m_server.credits = m_server.total_credits;
        }
        m_server.direction = idir;
    }
    m_server.flowmsg = msg;
    return &m_server;
}
