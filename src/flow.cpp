#include "zio/flow.hpp"

zio::flow::Flow::Flow(zio::portptr_t port)
    : m_port(port)
    , m_credits(0)
    , m_total_credits(0)
    , m_sender(true)
{
}
zio::flow::Flow::~Flow()
{
    // fixme: should we eot here?
}


void zio::flow::Flow::send_bot(zio::Message& bot)
{
    auto lobj = zio::json::parse(bot.label());
    lobj["flow"] = "BOT";
    bot.set_label(lobj.dump());
    m_port->send(bot);
}
bool zio::flow::Flow::recv_bot(zio::Message& bot, int timeout)
{
    bool ok = m_port->recv(bot, timeout);
    if (!ok) {
        zsys_warning("[flow %s]: timeout receiving BOT", m_port->name().c_str());
        return false;
    }
    auto lobj = zio::json::parse(bot.label());
    std::string flowtype = lobj["flow"];
    if (flowtype != "BOT") {
        zsys_warning("[flow %s]: did not get BOT, got %s",
                     m_port->name().c_str(), flowtype.c_str());
        return false;
    }
    m_total_credits = lobj["credits"];
    // lobj is from the point of view of the OTHER end
    std::string dir = lobj["direction"];
    if (dir == "extract") { 
        m_sender = false;
        m_credits = m_total_credits;
    }
    else if (dir == "inject") {
        m_sender = true;
        m_credits = 0;          
    }
    else {
        zsys_warning("[flow %s]: unknown direction: %s",
                     m_port->name().c_str(), dir.c_str());
        return false;
    }

    return true;
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
    auto obj = zio::json::parse(msg.label());
    std::string flowtype = obj["flow"];
    if (flowtype == "PAY") {
        int credit = obj["credit"];
        zsys_debug("flow::put() got %d credits", credit);
        return credit;
    }
    if (flowtype == "EOT") {
        return -1;
    }
    return -2;
}


bool zio::flow::Flow::put(zio::Message& dat)
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
    auto obj = zio::json::parse(dat.label());
    obj["flow"] = "DAT";
    dat.set_label(obj.dump());
    m_port->send(dat);
    --m_credits;
    return true;
}


bool zio::flow::Flow::get(zio::Message& dat, int timeout)
{
    if (m_credits) {
        Message msg("FLOW");
        zio::json obj{{"flow","PAY"},{"credit",m_credits}};
        msg.set_label(obj.dump());
        zsys_debug("flow::get() send %d credits", m_credits);
        m_credits=0;
        m_port->send(msg);
    }
    bool ok = m_port->recv(dat, timeout);
    if (!ok) { return false; }
    auto lobj = zio::json::parse(dat.label());
    if (lobj["flow"] != "DAT") {
        return false;
    }
    ++m_credits;
    return true;
}

bool zio::flow::Flow::eot(Message& msg, int timeout)
{
    auto obj = zio::json::parse(msg.label());
    obj["flow"] = "EOT";
    msg.set_label(obj.dump());

    m_port->send(msg);
    while (true) {
        bool ok = m_port->recv(msg, timeout);
        if (!ok) {              // timeout
            return false;
        }
        auto lobj = zio::json::parse(msg.label());
        if (lobj["flow"] != "EOT") {
            return true;
        }
    }
}




