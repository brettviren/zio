#include "zio/domo/client.hpp"
#include "zio/domo/protocol.hpp"
#include "zio/logging.hpp"

using namespace zio::domo;

Client::Client(zio::socket_t& sock, std::string broker_address)
    : m_sock(sock)
    , m_address(broker_address)
{
    int stype = m_sock.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_CLIENT == stype) {
        really_recv = recv_client;
        really_send = send_client;
    }
    else if (ZMQ_DEALER == stype) {
        really_recv = recv_dealer;
        really_send = send_dealer;
    }
    else {
        throw std::runtime_error(
            "client must be given DEALER or CLIENT socket");
    }

    connect_to_broker(false);
}

Client::~Client() {}

void Client::connect_to_broker(bool reconnect)
{
    if (reconnect) { m_sock.disconnect(m_address); }

    int linger = 0;
    m_sock.setsockopt(ZMQ_LINGER, linger);
    // set socket routing ID?
    m_sock.connect(m_address);
    zio::debug("zio::domo::Client connect to " + m_address);
}

void Client::send(std::string service, zio::multipart_t& request)
{
    request.pushstr(service);             // frame 2
    request.pushstr(mdp::client::ident);  // frame 1
    zio::debug("zio::domo::Client send request for " + service);
    really_send(m_sock, request, zio::send_flags::none);
}

void Client::recv(zio::multipart_t& reply)
{
    zio::poller_t<> poller;
    poller.add(m_sock, zio::event_flags::pollin);

    std::vector<zio::poller_event<> > events(1);
    int rc = poller.wait_all(events, m_timeout);
    if (rc > 0) {  // got one
        zio::multipart_t mmsg;
        really_recv(m_sock, mmsg, zio::recv_flags::none);

        std::string header = mmsg.popstr();
        assert(header == mdp::client::ident);

        std::string service = mmsg.popstr();
        reply = std::move(mmsg);
        return;  // success
    }
    if (interrupted()) {
        zio::error("zio::domo::Client interupted on recv");
        throw std::runtime_error("zio::domo::Client interupted on recv");
    }
    else {
        zio::error("zio::domo::Client timeout");
        throw std::runtime_error("zio::domo::Client timeout");
    }
    reply.clear();
    return;
}
