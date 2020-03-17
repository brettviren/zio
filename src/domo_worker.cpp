#include "zio/domo/worker.hpp"
#include "zio/domo/protocol.hpp"


using namespace zio::domo;

Worker::Worker(zmq::socket_t& sock, std::string broker_address,
               std::string service, logbase_t& log)
    : m_sock(sock)
    , m_address(broker_address)
    , m_service(service)
    , m_log(log)
{
    m_log.debug("zio::domo::Worker constructing on " + m_address);
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
        throw std::runtime_error("worker must be given DEALER or CLIENT socket");
    }

    connect_to_broker(false);
}

Worker::~Worker()
{
    m_log.debug("zio::domo::Worker destructing");
    m_sock.disconnect(m_address);
}

void Worker::connect_to_broker(bool reconnect)
{
    if (reconnect) {
        m_log.debug("zio::domo::Worker disconnect from " + m_address);
        m_sock.disconnect(m_address);
    }

    int linger=0;
    m_sock.setsockopt(ZMQ_LINGER, linger);
    // set socket routing ID?
    m_sock.connect(m_address);
    m_log.debug("zio::domo::Worker connect to " + m_address);

    zmq::multipart_t mmsg;
    mmsg.pushstr(m_service);          // 3
    mmsg.pushstr(mdp::worker::ready); // 2
    mmsg.pushstr(mdp::worker::ident); // 1
    really_send(m_sock, mmsg);

    m_liveness = HEARTBEAT_LIVENESS;
    m_heartbeat_at = now_ms() + m_heartbeat;
}

void Worker::send(zmq::multipart_t& reply)
{
    if (reply.empty()) {
        return;
    }
    reply.pushmem(NULL,0);             // 4
    reply.pushstr(m_reply_to);         // 3
    reply.pushstr(mdp::worker::reply); // 2
    reply.pushstr(mdp::worker::ident); // 1
    really_send(m_sock, reply);
}

void Worker::recv(zmq::multipart_t& request)
{
    zmq::poller_t<> poller;
    poller.add(m_sock, zmq::event_flags::pollin);

    std::vector< zmq::poller_event<> > events(1);
    int rc = poller.wait_all(events, m_heartbeat);
    if (rc > 0) {           // got one
        zmq::multipart_t mmsg;
        really_recv(m_sock, mmsg);
        m_liveness = HEARTBEAT_LIVENESS;
        std::string header = mmsg.popstr();  // 1
        assert(header == mdp::worker::ident);
        std::string command = mmsg.popstr(); // 2
        if (mdp::worker::request == command) {
            m_reply_to = mmsg.popstr(); // 3
            mmsg.pop();                 // 4
            request = std::move(mmsg);  // 5+
            return;                
        }
        else if (mdp::worker::heartbeat == command) {
            // nothing
        }
        else if (mdp::worker::disconnect == command) {
            connect_to_broker();
        }
        else {
            m_log.error("zio::domo::Worker invalid command: " + command);
        }
    }
    else {                  // timeout
        --m_liveness;
        if (m_liveness == 0) {
            m_log.debug("zio::domo::Worker disconnect from broker - retrying...");
        }
        sleep_ms(m_reconnect);
        connect_to_broker();
    }
    if (now_ms() >= m_heartbeat_at) {
        zmq::multipart_t mmsg;
        mmsg.pushstr(mdp::worker::heartbeat); // 2
        mmsg.pushstr(mdp::worker::ident);     // 1
        really_send(m_sock, mmsg);
        m_heartbeat_at += m_heartbeat;
    }

    return;
}

zmq::multipart_t Worker::work(zmq::multipart_t& reply)
{
    send(reply);

    while (! interrupted() ) {
        zmq::multipart_t request;
        recv(request);
        if (request.empty()) {
            continue;
        }
        return request;
    }
    if (interrupted()) {
        m_log.info("zio::domo::Worker interupt received, killing worker");
    }
    
    return zmq::multipart_t{};
}


void zio::domo::echo_worker(zmq::socket_t& pipe, std::string address, int socktype)
{
    // fixme: should get this from a more global spot.
    console_log log;

    // fixme: should implement BIND actor protocol 
    zmq::context_t ctx;
    zmq::socket_t sock(ctx, socktype);
    Worker worker(sock, address, "echo", log);
    log.debug("worker echo created on " + address);

    zmq::poller_t<> poller;
    poller.add(pipe, zmq::event_flags::pollin);
    poller.add(sock, zmq::event_flags::pollin);

    // we want to get back to our main loop often enough to check for
    // our creator to issue a termination (pipe hit) but not so fast
    // that the loop spins and wastes CPU.
    time_unit_t poll_resolution{500};

    pipe.send(zmq::message_t{}, zmq::send_flags::none); // ready

    log.debug("worker echo starting");
    zmq::multipart_t reply;
    while ( ! interrupted() ) {

        log.debug("worker check pipe");
        std::vector< zmq::poller_event<> > events(2);
        int nevents = poller.wait_all(events, poll_resolution);
        for (int iev=0; iev < nevents; ++iev) {

            if (events[iev].socket == pipe) {
                log.debug("worker pipe hit");
                return;
            }

            if (events[iev].socket == sock) {
                log.debug("worker echo work");
                zmq::multipart_t request;
                worker.recv(request);
                if (request.empty()) {
                    log.debug("worker echo got null request");
                    break;
                }
                reply = std::move(request);
                worker.send(reply);
            }
        }
    }
    // fixme: should poll on pipe to check for early shutdown
    log.debug("worker echo wait for term");    
    zmq::message_t die;
    auto res = pipe.recv(die, zmq::recv_flags::none);
    log.debug("worker echo wait for exit");    
}
