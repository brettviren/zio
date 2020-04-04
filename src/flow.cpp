#include "zio/flow.hpp"
#include "zio/logging.hpp"

const char* zio::flow::name(MessageType mt)
{
    const char* names[] = { "timeout", "BOT", "DAT", "PAY", "EOT", "notype" };
    return names[mt];
}

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


bool zio::flow::parse_label(Message& msg, zio::json& lobj)
{
    std::string label = msg.label();
    if (label.empty()) {
        return true;
    }
    // zio::info("parse_label({})", label);
    try {
        lobj = zio::json::parse(label);
    }
    catch (zio::json::exception& e) {
        // zio::warn("[flow {}]: {}",
        //           m_port->name(), e.what());
        // zio::warn("[flow {}]: {}",
        //           m_port->name(), label);
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
               m_port->name(), zio::binstr(m_remid));
    zio::json fobj;
    if (!zio::flow::parse_label(bot, fobj)) {
        throw std::runtime_error("bad message label for flow::send_bot()");
    }
    fobj["flow"] = "BOT";
    bot.set_seqno(m_send_seqno = 0);
    bot.set_label(fobj.dump());
    bot.set_form("FLOW");
    if (m_remid.size()) { bot.set_remote_id(m_remid); }
    m_port->send(bot);
}


zio::flow::MessageType zio::flow::Flow::recv_bot(zio::Message& msg, int timeout)
{
    zio::debug("[flow {}]: recv_bot", m_port->name());
    return recv(msg, timeout);
}

zio::flow::MessageType zio::flow::Flow::proc_bot(zio::Message& bot, const zio::json& fobj)
{
    // here, fobj starts from the point of view of the OTHER end
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
                     m_port->name(), dir);
        return zio::flow::notype;
    }
    m_remid = bot.remote_id();
    if (m_remid.size()) {
        zio::debug("[flow {}]: remote id: {}",
                   m_port->name(), zio::binstr(m_remid));
    }

    return zio::flow::BOT;
}



zio::flow::MessageType zio::flow::Flow::recv_pay(zio::Message& msg, int timeout)
{
    zio::debug("[flow {}]: recv_pay", m_port->name());
    return recv(msg, timeout);
}

zio::flow::MessageType zio::flow::Flow::proc_pay(zio::Message& pay, const zio::json& fobj)
{
    int credit = fobj["credit"];
    m_credit += credit;
    zio::debug("[flow {}] recv PAY {} have {} credit",
               m_port->name(), credit, m_credit);
    return zio::flow::PAY;
}


zio::flow::MessageType zio::flow::Flow::put(zio::Message& dat, int timeout)
{
    if (m_send_seqno < 0) {
        zio::error("[flow {}] send DAT #{}",
                   m_port->name(), m_send_seqno);
        throw std::runtime_error("flow::put() must send BOT first");
    }

    zio::json fobj;
    if (!zio::flow::parse_label(dat, fobj)) {
        throw std::runtime_error("bad message label for Flow::put()");
    }

    zio::debug("[flow {}] put() with {} credit", m_port->name(), m_credit);

    // Slurp in any waiting PAY
    while (m_credit < m_total_credit) {
        Message pay;
        auto mtype = recv_pay(pay, 0);
        if (mtype == zio::flow::timeout) {
            break;
        }
        if (mtype == zio::flow::EOT) {
            return mtype;
        }
    }

    // If not credit we must not send DAT
    while (m_credit == 0) {

        Message pay;
        auto mtype = recv_pay(pay, timeout);
        if (mtype == zio::flow::timeout) {
            return mtype;
        }
        if (mtype == zio::flow::EOT) {
            return mtype;
        }

    }

    fobj["flow"] = "DAT";
    dat.set_label(fobj.dump());
    dat.set_form("FLOW");
    dat.set_seqno(++m_send_seqno);
    if (m_remid.size()) { dat.set_remote_id(m_remid); }
    m_port->send(dat);
    --m_credit;
    return zio::flow::DAT;
}

int zio::flow::Flow::flush_pay()
{
    if (!m_credit) {
        zio::debug("[flow {}] no PAY to flush (out of {})", 
                   m_port->name(), m_total_credit);
        return 0;
    }
    Message msg("FLOW");
    zio::json obj{{"flow","PAY"},{"credit",m_credit}};
    msg.set_label(obj.dump());
    msg.set_seqno(++m_send_seqno);
    zio::debug("[flow {}] send PAY #{}, credit:{} (remid:{})",
               m_port->name(), m_send_seqno, m_credit, zio::binstr(m_remid));
    const int nsent = m_credit;
    m_credit=0;
    if (m_remid.size()) { msg.set_remote_id(m_remid); }
    m_port->send(msg);

    return nsent;
}

zio::flow::MessageType zio::flow::Flow::recv_dat(zio::Message& msg, int timeout)
{
    flush_pay();
    zio::debug("[flow {}] get with {} credit (remid:{})",
               m_port->name(), m_credit, zio::binstr(m_remid));

    return recv(msg, timeout);
}    

zio::flow::MessageType zio::flow::Flow::proc_dat(zio::Message& dat, const zio::json& fobj)
{
    ++m_credit;
    return zio::flow::DAT;
}


void zio::flow::Flow::send_eot(Message& msg)
{
    msg.set_form("FLOW");
    zio::json fobj;
    if (!zio::flow::parse_label(msg, fobj)) {
        throw std::runtime_error("bad message label for Flow::send_eot()");
    }
    fobj["flow"] = "EOT";
    msg.set_label(fobj.dump());
    msg.set_seqno(++m_send_seqno);
    if (m_remid.size()) { msg.set_remote_id(m_remid); }
    m_port->send(msg);
}

zio::flow::MessageType zio::flow::Flow::recv_eot(Message& msg, int timeout)
{
    zio::debug("[flow {}]: recv_eot", m_port->name());
    return recv(msg, timeout);
}

zio::flow::MessageType zio::flow::Flow::proc_eot(zio::Message& eot, const zio::json& fobj)
{
    // nothing to do
    return zio::flow::EOT;
}

zio::flow::MessageType zio::flow::Flow::finish(Message& msg, int timeout)
{
    while (true) {
        auto mtype = recv(msg, timeout);
        if (mtype == zio::flow::timeout or mtype == zio::flow::EOT) {
            return mtype;
        }
        zio::debug("[flow {}]: close discarding {}",
                   m_port->name(), zio::flow::name(mtype));
    }
    return zio::flow::notype;
}    
zio::flow::MessageType zio::flow::Flow::close(Message& msg, int timeout)
{
    try {
        send_eot(msg);
    }
    catch (zmq::error_t) {
        return zio::flow::EOT;
    }
    return finish(msg, timeout);
}

zio::flow::MessageType zio::flow::type(Message& msg)
{
    zio::json fobj;
    if (!zio::flow::parse_label(msg, fobj)) {
        return zio::flow::notype;
    }
    std::string flowtype = fobj["flow"];

    if (flowtype == "BOT") {
        return zio::flow::BOT;
    }
    if (flowtype == "DAT") {
        return zio::flow::DAT;
    }
    if (flowtype == "PAY") {
        return zio::flow::PAY;
    }
    if (flowtype == "EOT") {
        return zio::flow::EOT;
    }
    return zio::flow::notype;
}

zio::flow::MessageType zio::flow::Flow::recv_one(Message& msg, int timeout)
{
    bool ok = m_port->recv(msg, timeout);
    if (!ok) {
        return zio::flow::timeout;
    }

    // check but do not set recv_seqno
    if (msg.seqno() - m_recv_seqno != 1) {
        zio::warn("[flow {}]: msg with bad seqno: {}",
                  m_port->name(), msg.seqno());
        return zio::flow::notype;
    }

    zio::json fobj;
    if (!zio::flow::parse_label(msg, fobj)) {
        return zio::flow::notype;
    }
    std::string flowtype = fobj["flow"];

    zio::flow::MessageType mtype = zio::flow::notype;
    if (flowtype == "BOT") {
        mtype = proc_bot(msg, fobj);
    }
    else if (flowtype == "DAT") {
        mtype = proc_dat(msg, fobj);
    }
    else if (flowtype == "PAY") {
        mtype = proc_pay(msg, fobj);
    }
    else if (flowtype == "EOT") {
        mtype = proc_eot(msg, fobj);
    }
    return mtype;
}

zio::flow::MessageType zio::flow::Flow::recv(Message& msg, int timeout)
{
    // fixme: for postive timeout, accumulte time spent 
    while (true) {
        auto mtype = recv_one(msg, timeout);
        if (mtype == zio::flow::notype) {
            continue;
        }
        if (mtype == zio::flow::timeout) {
            return mtype;
        }
        ++m_recv_seqno;             // success
        return mtype;
    }
    return zio::flow::notype;    
}
