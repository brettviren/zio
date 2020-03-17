#include "zio/domo/client.hpp"
#include "zio/domo/protocol.hpp"

using namespace zio::domo;

Client::Client(zmq::socket_t& sock, std::string broker_address,
               logbase_t& log)
    : m_sock(sock)
    , m_address(broker_address)
    , m_log(log)
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
        throw std::runtime_error("client must be given DEALER or CLIENT socket");
    }

    connect_to_broker(false);
}


Client::~Client() { } 

void Client::connect_to_broker(bool reconnect)
{
    if (reconnect) {
        m_sock.disconnect(m_address);
    }

    int linger=0;
    m_sock.setsockopt(ZMQ_LINGER, linger);
    // set socket routing ID?
    m_sock.connect(m_address);
    m_log.debug("zio::domo::Client connect to " + m_address);
}


void Client::send(std::string service, zmq::multipart_t& request)
{
    request.pushstr(service);            // frame 2
    request.pushstr(mdp::client::ident); // frame 1
    m_log.debug("zio::domo::Client send request for " + service);
    really_send(m_sock, request);
}



void Client::recv(zmq::multipart_t& reply)
{
    zmq::poller_t<> poller;
    poller.add(m_sock, zmq::event_flags::pollin);
        
    std::vector< zmq::poller_event<> > events(1);
    int rc = poller.wait_all(events, m_timeout);
    if (rc > 0) {           // got one
        zmq::multipart_t mmsg;
        really_recv(m_sock, mmsg);

        std::string header = mmsg.popstr();
        assert(header == mdp::client::ident);
            
        std::string service = mmsg.popstr();
        reply = std::move(mmsg);
        return;                 // success
    }
    if ( interrupted() ) {
        m_log.error("zio::domo::Client interupted on recv");
    }
    else {
        m_log.error("zio::domo::Client timeout");
    }
    reply.clear();
    return;
}


