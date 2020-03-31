#include "zio/flow.hpp"
#include "zio/logging.hpp"

zio::flow::Flow::Flow(zio::portptr_t port)
    : m_port(port)
    , m_credit(0)
    , m_total_credit(0)
    , m_sender(true)
    , m_remid("")
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
    // zio::info("parse_label({})", label.c_str());
    try {
        lobj = zio::json::parse(label);
    }
    catch (zio::json::exception& e) {
        zio::warn("[flow {}]: {}",
                     m_port->name().c_str(), e.what());
        zio::warn("[flow {}]: {}",
                     m_port->name().c_str(), label.c_str());
        return false;
    }
    return true;
}


void zio::flow::Flow::send_bot(zio::Message& bot)
{
    if (m_send_seqno != -1) {
        throw std::runtime_error("flow::send_bot() already called");
    }

    zio::debug("[flow {}]: send_bot to {}",
               m_port->name().c_str(), zio::binstr(m_remid));
    zio::json fobj;
    if (!parse_label(bot, fobj)) {
        throw std::runtime_error("bad message label for flow::send_bot()");
    }
    fobj["flow"] = "BOT";
    bot.set_seqno(m_send_seqno = 0);
    bot.set_label(fobj.dump());
    bot.set_form("FLOW");
    if (m_remid.size()) { bot.set_remote_id(m_remid); }
    m_port->send(bot);
}


bool zio::flow::Flow::recv_bot(zio::Message& bot, int timeout)
{
    zio::debug("[flow {}]: recv_bot", m_port->name().c_str());
    bool ok = m_port->recv(bot, timeout);
    if (!ok) {
        zio::warn("[flow {}]: timeout receiving BOT",
                     m_port->name().c_str());
        return false;
    }
    std::string label = bot.label();
    if (bot.seqno()) {
        zio::warn("[flow {}]: bad BOT seqno: {}, label: {}",
                     m_port->name().c_str(), bot.seqno(), label.c_str());
        return false;
    }

    zio::debug("[flow {}]: seqno: {}, label: {}",
               m_port->name().c_str(), bot.seqno(), label.c_str());

    zio::json fobj;
    if (!parse_label(bot, fobj)) {
        zio::warn("bad message label for flow::recv_bot()");
        return false;
    }
    std::string flowtype = fobj["flow"];
    if (flowtype != "BOT") {
        zio::warn("[flow {}]: did not get BOT, got {}",
                     m_port->name().c_str(), flowtype.c_str());
        return false;
    }
    // here, fobj is from the point of view of the OTHER end
    std::string dir = fobj["direction"];
    if (dir == "extract") { 
        m_sender = false;       // we are receiver
        m_total_credit = fobj["credit"];
        m_credit = m_total_credit;
    }
    else if (dir == "inject") {
        m_sender = true;
        m_total_credit = fobj["credit"];
        m_credit = 0;          
    }
    else {
        zio::warn("[flow {}]: unknown direction: {}",
                     m_port->name().c_str(), dir.c_str());
        return false;
    }
    m_recv_seqno = bot.seqno();
    m_remid = bot.remote_id();
    if (m_remid.size()) {
        zio::debug("[flow {}]: remote id: {}",
                   m_port->name().c_str(), zio::binstr(m_remid));
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
    if (msg.seqno() - m_recv_seqno != 1) {
        zio::warn("[flow {}] slurp_pay: bad seqno: {}, last seqno: {}, {}",
                  m_port->name(), msg.seqno(), m_recv_seqno, msg.label());
        return slurp_pay(timeout);
    }

    zio::json fobj;
    if (!parse_label(msg, fobj)) {
        zio::warn("[flow {}] slurp_pay: bad flow object: {}",
                  m_port->name(), msg.label());
        return slurp_pay(timeout);
    }

    std::string flowtype = fobj["flow"];
    if (flowtype == "PAY") {
        int credit = fobj["credit"];
        zio::debug("[flow {}] recv PAY {} credit (remid:{})",
                   m_port->name().c_str(), credit, zio::binstr(m_remid));
        ++m_recv_seqno;
        return credit;
    }
    if (flowtype == "EOT") {
        zio::warn("[flow {}] slurp_pay: EOT seqno: {}, last seqno: {}, {}",
                  m_port->name(), msg.seqno(), m_recv_seqno, msg.label());
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
        m_credit = c;
    }

    if (m_send_seqno < 0) {
        zio::error("[flow {}] send DAT {}",
                   m_port->name().c_str(), m_send_seqno);
        throw std::runtime_error("flow::put() must send BOT first");
    }

    zio::json fobj;
    if (!parse_label(dat, fobj)) {
        throw std::runtime_error("bad message label for Flow::put()");
    }

    fobj["flow"] = "DAT";
    dat.set_label(fobj.dump());
    dat.set_form("FLOW");
    dat.set_seqno(++m_send_seqno);
    if (m_remid.size()) { dat.set_remote_id(m_remid); }
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
    msg.set_seqno(++m_send_seqno);
    zio::debug("[flow {}] send PAY {}, credit:{} (remid:{})",
               m_port->name().c_str(), m_send_seqno, m_credit, zio::binstr(m_remid));
    const int nsent = m_credit;
    m_credit=0;
    if (m_remid.size()) { msg.set_remote_id(m_remid); }
    m_port->send(msg);

    return nsent;
}

bool zio::flow::Flow::get(zio::Message& dat, int timeout)
{
    flush_pay();
    zio::debug("[flow {}] get with {} credit (remid:{})",
               m_port->name().c_str(), m_credit, zio::binstr(m_remid));

    bool ok = m_port->recv(dat, timeout);
    if (!ok) {
        zio::warn("[flow {}] get: no recv seqno: {}, last seqno: {}, {}",
                  m_port->name(), dat.seqno(), m_recv_seqno, dat.label());
        return false;
    }

    if (dat.seqno() - m_recv_seqno != 1) {
        zio::warn("[flow {}] get: bad seqno: {}, last seqno: {}, {}",
                  m_port->name(), dat.seqno(), m_recv_seqno, dat.label());
        return false;
    }
    zio::json fobj;
    if (!parse_label(dat, fobj)) {
        zio::warn("[flow {}] get: bad label seqno: {}, last seqno: {}, {}",
                  m_port->name(), dat.seqno(), m_recv_seqno, dat.label());
        return false;
    }
    if (fobj["flow"] != "DAT") {
        zio::warn("[flow {}] get: not a DAQ seqno: {}, last seqno: {}, {}",
                  m_port->name(), dat.seqno(), m_recv_seqno, dat.label());
        return false;
    }
    ++m_recv_seqno;
    ++m_credit;
    return true;
}

void zio::flow::Flow::send_eot(Message& msg)
{
    msg.set_form("FLOW");
    zio::json fobj;
    if (!parse_label(msg, fobj)) {
        throw std::runtime_error("bad message label for Flow::send_eot()");
    }
    fobj["flow"] = "EOT";
    msg.set_label(fobj.dump());
    msg.set_seqno(++m_send_seqno);
    if (m_remid.size()) { msg.set_remote_id(m_remid); }
    m_port->send(msg);
}

bool zio::flow::Flow::recv_eot(Message& msg, int timeout)
{
    while (true) {
        bool ok = m_port->recv(msg, timeout);
        if (!ok) {              // timeout
            return false;
        }
        if (msg.seqno() - m_recv_seqno != 1) {
            zio::warn("[flow {}] recv_eot: bad seqno: {}, last seqno: {}, {}",
                      m_port->name(), msg.seqno(), m_recv_seqno, msg.label());
            return false;
        }
        zio::json fobj;
        if (!parse_label(msg, fobj)) {
            zio::warn("bad message label for Flow::recv_eot()");
            return false;
        }
        std::string flowtype = fobj["flow"];
        ++m_recv_seqno;
        if (flowtype == "EOT") {
            return true;
        }
        zio::debug("[flow {}] want EOT got {} (remid:{})",
                   m_port->name().c_str(), flowtype.c_str(), m_remid);
    }
}




