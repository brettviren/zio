#include "zio/extract.hpp"

zio::ExtractClient::ExtractClient(portptr_t port, int credits, zio::json extra)
    : m_port(port)
    , m_credits(0), m_total_credits(credits)
    , m_credmsg({{level::undefined, "CRED", ""},{0,0,0}})
    , m_done{false}
{
    m_credmsg.set_label("REQ");
    extra["credit"] = credits;
    extra["request"] = "extract";
    std::string s = extra.dump();
    m_credmsg.payload().push_back(payload_t(s.begin(), s.end()));
    port->send(m_credmsg);
}
zio::ExtractClient::~ExtractClient()
{
    // Caller didn't explicitly send end of transmission, do best
    // effort to send it just before we destruct, but don't wait.
    if (!m_done) {
        eot(0);
    }
}

bool zio::ExtractClient::slurp_credit(int timeout)
{
    while (m_credits < m_total_credits) {
        bool ok = m_port->recv(m_credmsg, timeout);
        if (!ok) {              // timeout
            return false;
        }
        std::string label = m_credmsg.header().prefix.label;
        if (label != "PAY") {
            zsys_warning("[port %s]: unexpected CRED message type: %s",
                         m_port->name().c_str(), label.c_str());
            continue;
        }
        auto& ml = m_credmsg.payload();

        int pay = 1;
        if (ml.size() > 0) {
            auto& pl = ml[0];
            std::string s(pl.begin(), pl.end());
            auto j = json::parse(s);
            if (j["credit"].is_number()) {
                pay = j["credit"];
            }
        }
        m_credits += pay;
    }
    return true;
}

void zio::ExtractClient::send(Message& msg)
{
    slurp_credit(0);
    if (!m_credits) {
        slurp_credit(-1);       // wait forever
    }

    m_port->send(msg);
    --m_credits;
}

bool zio::ExtractClient::eot(int timeout)
{
    if (m_done) { return true; }

    m_credmsg.set_label("EOT");    
    m_credmsg.payload().clear();
    m_port->send(m_credmsg);

    return slurp_credit(timeout);
}

