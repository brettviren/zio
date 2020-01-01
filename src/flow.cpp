#include "zio/flow.hpp"

zio::flow::Flow::Flow(zio::portptr_t port)
    : m_port(port)
    , m_credits(0)
    , m_total_credits(0)
    , m_sender(true)
    , m_rid(0)
{
}
zio::flow::Flow::~Flow()
{
    // fixme: should we eot here?
}


void zio::flow::Flow::send_bot(zio::Message& bot)
{
    zsys_debug("[flow %s]: send_bot", m_port->name().c_str());
    auto lobj = zio::json::parse(bot.label());
    lobj["flow"] = "BOT";
    bot.set_label(lobj.dump());
    if (m_rid) { bot.set_routing_id(m_rid); }
    m_port->send(bot);
}
bool zio::flow::Flow::recv_bot(zio::Message& bot, int timeout)
{
    zsys_debug("[flow %s]: recv_bot", m_port->name().c_str());
    bool ok = m_port->recv(bot, timeout);
    if (!ok) {
        zsys_warning("[flow %s]: timeout receiving BOT",
                     m_port->name().c_str());
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
    // here, lobj is from the point of view of the OTHER end
    std::string dir = lobj["direction"];
    if (dir == "extract") { 
        m_sender = false;       // we are receiver
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
    m_rid = bot.routing_id();
    if (m_rid) {
        zsys_debug("[flow %s]: routing id: %u",
                   m_port->name().c_str(), m_rid);
    }

    return true;
}



int zio::flow::Flow::slurp_pay(int timeout)
{
    zio::Message msg;
    bool ok = m_port->recv(msg, timeout);
    if (!ok) {              // timeout
        return 0;
    }
    auto obj = zio::json::parse(msg.label());
    std::string flowtype = obj["flow"];
    if (flowtype == "PAY") {
        int credit = obj["credit"];
        zsys_debug("[flow %s] recv PAY %d credits (rid:%u)",
                   m_port->name().c_str(), credit, m_rid);
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
        int c = slurp_pay(0);
        if (c < 0) {
            return false;
        }
        m_credits += c;
    }
    if (m_credits == 0) {
        // not credits, we really have to wait until we get some PAY
        int c = slurp_pay(-1);
        if (c < 0) {
            return false;
        }
        assert (c>0);
        m_credits = c;
    }
    auto obj = zio::json::parse(dat.label());
    obj["flow"] = "DAT";
    dat.set_label(obj.dump());
    if (m_rid) { dat.set_routing_id(m_rid); }
    m_port->send(dat);
    --m_credits;
    return true;
}

int zio::flow::Flow::flush_pay()
{
    if (!m_credits) {
        return 0;
    }
    Message msg("FLOW");
    zio::json obj{{"flow","PAY"},{"credit",m_credits}};
    msg.set_label(obj.dump());
    zsys_debug("[flow %s] send PAY %d credits (rid:%u)",
               m_port->name().c_str(), m_credits, m_rid);
    const int nsent = m_credits;
    m_credits=0;
    if (m_rid) { msg.set_routing_id(m_rid); }
    m_port->send(msg);

    return nsent;
}

bool zio::flow::Flow::get(zio::Message& dat, int timeout)
{
    zsys_debug("[flow %s] get with %d credits (rid:%u)",
               m_port->name().c_str(), m_credits, m_rid);

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

    if (m_rid) { msg.set_routing_id(m_rid); }
    m_port->send(msg);
    while (true) {
        bool ok = m_port->recv(msg, timeout);
        if (!ok) {              // timeout
            return false;
        }
        auto lobj = zio::json::parse(msg.label());
        std::string flowtype = lobj["flow"];
        if (flowtype == "EOT") {
            return true;
        }
        zsys_debug("[flow %s] want EOT got %s (rid:%u)",
                   m_port->name().c_str(), flowtype.c_str(), m_rid);
    }
}




