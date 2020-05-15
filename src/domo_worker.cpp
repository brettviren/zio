#include "zio/domo/worker.hpp"
#include "zio/domo/protocol.hpp"
#include "zio/logging.hpp"

using namespace zio::domo;

Worker::Worker(zio::socket_t& sock, std::string broker_address,
               std::string service)
    : m_sock(sock)
    , m_address(broker_address)
    , m_service(service)
{
    zio::debug("zio::domo::Worker constructing on " + m_address);
    int stype = m_sock.get(zmq::sockopt::type);
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
            "worker must be given DEALER or CLIENT socket");
    }

    connect_to_broker(false);
}

Worker::~Worker()
{
    zio::debug("zio::domo::Worker destructing");
    m_sock.disconnect(m_address);
}

void Worker::connect_to_broker(bool reconnect)
{
    if (reconnect) {
        zio::debug("zio::domo::Worker disconnect from " + m_address);
        m_sock.disconnect(m_address);
    }

    int linger = 0;
    m_sock.set(zmq::sockopt::linger, linger);
    // set socket routing ID?
    m_sock.connect(m_address);
    zio::debug("zio::domo::Worker connect to " + m_address);

    zio::multipart_t mmsg;
    mmsg.pushstr(m_service);           // 3
    mmsg.pushstr(mdp::worker::ready);  // 2
    mmsg.pushstr(mdp::worker::ident);  // 1
    really_send(m_sock, mmsg, zio::send_flags::none);

    m_liveness = HEARTBEAT_LIVENESS;
    m_heartbeat_at = now_ms() + m_heartbeat;
}

void Worker::send(zio::multipart_t& reply)
{
    if (reply.empty()) { return; }
    reply.pushmem(NULL, 0);             // 4
    reply.pushstr(m_reply_to);          // 3
    reply.pushstr(mdp::worker::reply);  // 2
    reply.pushstr(mdp::worker::ident);  // 1
    really_send(m_sock, reply, zio::send_flags::none);
}

void Worker::recv(zio::multipart_t& request)
{
    zio::poller_t<> poller;
    poller.add(m_sock, zio::event_flags::pollin);

    std::vector<zio::poller_event<> > events(1);
    int rc = poller.wait_all(events, m_heartbeat);
    if (rc > 0) {  // got one
        zio::multipart_t mmsg;
        really_recv(m_sock, mmsg, zio::recv_flags::none);
        m_liveness = HEARTBEAT_LIVENESS;
        std::string header = mmsg.popstr();  // 1
        assert(header == mdp::worker::ident);
        std::string command = mmsg.popstr();  // 2
        if (mdp::worker::request == command) {
            m_reply_to = mmsg.popstr();  // 3
            mmsg.pop();                  // 4
            request = std::move(mmsg);   // 5+
            return;
        }
        else if (mdp::worker::heartbeat == command) {
            // nothing
        }
        else if (mdp::worker::disconnect == command) {
            connect_to_broker();
        }
        else {
            zio::warn("zio::domo::Worker invalid command: " + command);
        }
    }
    else {  // timeout
        --m_liveness;
        if (m_liveness == 0) {
            zio::debug(
                "zio::domo::Worker disconnect from broker - retrying...");
        }
        sleep_ms(m_reconnect);
        connect_to_broker();
    }
    if (now_ms() >= m_heartbeat_at) {
        zio::multipart_t mmsg;
        mmsg.pushstr(mdp::worker::heartbeat);  // 2
        mmsg.pushstr(mdp::worker::ident);      // 1
        really_send(m_sock, mmsg, zio::send_flags::none);
        m_heartbeat_at += m_heartbeat;
    }

    return;
}

zio::multipart_t Worker::work(zio::multipart_t& reply)
{
    send(reply);

    while (!interrupted()) {
        zio::multipart_t request;
        recv(request);
        if (request.empty()) { continue; }
        return request;
    }
    if (interrupted()) {
        zio::info("zio::domo::Worker interupt received, killing worker");
    }

    return zio::multipart_t{};
}

void zio::domo::echo_worker(zio::socket_t& link, std::string address,
                            int socktype)
{
    // fixme: should implement BIND actor protocol
    zio::context_t ctx;
    zio::socket_t sock(ctx, socktype);
    Worker worker(sock, address, "echo");
    zio::debug("worker echo created on " + address);

    zio::poller_t<> poller;
    poller.add(link, zio::event_flags::pollin);
    poller.add(sock, zio::event_flags::pollin);

    // we want to get back to our main loop often enough to check for
    // our creator to issue a termination (link hit) but not so fast
    // that the loop spins and wastes CPU.
    time_unit_t poll_resolution{500};

    link.send(zio::message_t{}, zio::send_flags::none);  // ready

    zio::debug("worker echo starting");
    zio::multipart_t reply;
    while (!interrupted()) {
        zio::debug("worker check link");
        std::vector<zio::poller_event<> > events(2);
        int nevents = poller.wait_all(events, poll_resolution);
        for (int iev = 0; iev < nevents; ++iev) {
            if (events[iev].socket == link) {
                zio::debug("worker link hit");
                return;
            }

            if (events[iev].socket == sock) {
                zio::debug("worker echo work");
                zio::multipart_t request;
                worker.recv(request);
                if (request.empty()) {
                    zio::warn("worker echo got null request");
                    break;
                }
                reply = std::move(request);
                worker.send(reply);
            }
        }
    }
    // fixme: should poll on link to check for early shutdown
    zio::debug("worker echo wait for term");
    zio::message_t die;
    auto res = link.recv(die, zio::recv_flags::none);
    res = {};  // don't care
    zio::debug("worker echo wait for exit");
}
