#include "zio/flow.hpp"

zio::flow::Flow::Flow(zio::portptr_t port)
    : m_port(port)
    , m_credit(0)
    , m_total_credit(0)
    , m_sender(true)
    , m_rid(0)
{
}
zio::flow::Flow::~Flow()
{
    // fixme: should we eot here?
}


bool zio::flow::Flow::parse_label(Message& msg, zio::json& lobj)
{
    std::string label = msg.label();
    if (label.empty()) {
        return true;
    }
    try {
        lobj = zio::json::parse(label);
    }
    catch (zio::json::exception& e) {
        zsys_warning("[flow %s]: %s",
                     m_port->name().c_str(), e.what());
        zsys_warning("[flow %s]: %s",
                     m_port->name().c_str(), label.c_str());
        return false;
    }
    return true;
}


void zio::flow::Flow::send_bot(zio::Message& bot)
{
    zsys_debug("[flow %s]: send_bot", m_port->name().c_str());
    zio::json fobj;
    if (!parse_label(bot, fobj)) {
        throw std::runtime_error("bad message label for flow::send_bot()");
    }
    fobj["flow"] = "BOT";
    bot.set_label(fobj.dump());
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
    std::string label = bot.label();
    zsys_debug("[flow %s]: label: %s",
               m_port->name().c_str(), label.c_str());

    zio::json fobj;
    if (!parse_label(bot, fobj)) {
        zsys_warning("bad message label for flow::recv_bot()");
        return false;
    }
    std::string flowtype = fobj["flow"];
    if (flowtype != "BOT") {
        zsys_warning("[flow %s]: did not get BOT, got %s",
                     m_port->name().c_str(), flowtype.c_str());
        return false;
    }
    m_total_credit = fobj["credit"];
    // here, fobj is from the point of view of the OTHER end
    std::string dir = fobj["direction"];
    if (dir == "extract") { 
        m_sender = false;       // we are receiver
        m_credit = m_total_credit;
    }
    else if (dir == "inject") {
        m_sender = true;
        m_credit = 0;          
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
    zio::json fobj;
    if (!parse_label(msg, fobj)) {
        return 0;
    }
    std::string flowtype = fobj["flow"];
    if (flowtype == "PAY") {
        int credit = fobj["credit"];
        zsys_debug("[flow %s] recv PAY %d credit (rid:%u)",
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
    if (m_credit < m_total_credit) {
        // quick try to get any PAY already sitting in buffers
        int c = slurp_pay(0);
        if (c < 0) {
            return false;
        }
        m_credit += c;
    }
    if (m_credit == 0) {
        // no credit, we really have to wait until we get some PAY
        int c = slurp_pay(-1);
        if (c < 0) {
            return false;
        }
        assert (c>0);
        m_credit = c;
    }

    zio::json fobj;
    if (!parse_label(dat, fobj)) {
        throw std::runtime_error("bad message label for Flow::put()");
    }

    fobj["flow"] = "DAT";
    dat.set_label(fobj.dump());
    if (m_rid) { dat.set_routing_id(m_rid); }
    m_port->send(dat);
    --m_credit;
    return true;
}

int zio::flow::Flow::flush_pay()
{
    if (!m_credit) {
        return 0;
    }
    Message msg("FLOW");
    zio::json obj{{"flow","PAY"},{"credit",m_credit}};
    msg.set_label(obj.dump());
    zsys_debug("[flow %s] send PAY %d credit (rid:%u)",
               m_port->name().c_str(), m_credit, m_rid);
    const int nsent = m_credit;
    m_credit=0;
    if (m_rid) { msg.set_routing_id(m_rid); }
    m_port->send(msg);

    return nsent;
}

bool zio::flow::Flow::get(zio::Message& dat, int timeout)
{
    zsys_debug("[flow %s] get with %d credit (rid:%u)",
               m_port->name().c_str(), m_credit, m_rid);

    bool ok = m_port->recv(dat, timeout);
    if (!ok) { return false; }
    zio::json fobj;
    if (!parse_label(dat, fobj)) {
        throw std::runtime_error("bad message label for Flow::get()");
    }
    if (fobj["flow"] != "DAT") {
        return false;
    }
    ++m_credit;
    return true;
}

void zio::flow::Flow::send_eot(Message& msg)
{
    zio::json fobj;
    if (!parse_label(msg, fobj)) {
        throw std::runtime_error("bad message label for Flow::send_eot()");
    }
    fobj["flow"] = "EOT";
    msg.set_label(fobj.dump());

    if (m_rid) { msg.set_routing_id(m_rid); }
    m_port->send(msg);
}

bool zio::flow::Flow::recv_eot(Message& msg, int timeout)
{
    while (true) {
        bool ok = m_port->recv(msg, timeout);
        if (!ok) {              // timeout
            return false;
        }
        zio::json fobj;
        if (!parse_label(msg, fobj)) {
            throw std::runtime_error("bad message label for Flow::send_eot()");
        }
        std::string flowtype = fobj["flow"];
        if (flowtype == "EOT") {
            return true;
        }
        zsys_debug("[flow %s] want EOT got %s (rid:%u)",
                   m_port->name().c_str(), flowtype.c_str(), m_rid);
    }
}




