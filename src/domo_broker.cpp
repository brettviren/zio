// The broker code is implemented closely following the C++ example
// for Majordomo in the Zguide (mdbroker.cpp).  However, we use cppzmq
// and abstract away the differences between ROUTER and SERVER

#include "zio/domo/broker.hpp"
#include "zio/domo/protocol.hpp"
#include "zio/util.hpp"
#include <sstream>

using namespace zio::domo;


Broker::Service::~Service () {
}


Broker::Broker(zmq::socket_t& sock, logbase_t& log)
    : m_sock(sock)
    , m_log(log)
{
    int stype = m_sock.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_SERVER == stype) {
        recv = recv_server;
        send = send_server;
        m_log.info("zio::domo::Broker with SERVER starting");
        return;
    }
    if(ZMQ_ROUTER == stype) {
        recv = recv_router;
        send = send_router;
        m_log.info("zio::domo::Broker with ROUTER starting");
        return;
    }
    throw std::runtime_error("zio::domo::Broker requires SERVER or ROUTER socket");
}

Broker::~Broker()
{
    while (! m_services.empty()) {
        delete m_services.begin()->second;
        m_services.erase(m_services.begin());
    }
    while (! m_workers.empty()) {
        delete m_workers.begin()->second;
        m_workers.erase(m_workers.begin());
    }
}


void Broker::proc_one()
{
    zmq::multipart_t mmsg;
    remote_identity_t sender = recv(m_sock, mmsg);
    assert(mmsg.size() > 0);
    std::string header = mmsg.popstr(); // 7/MDP frame 1
    if (header == mdp::client::ident) {
        m_log.debug("zio::domo::Broker process client");
        client_process(sender, mmsg);
    }
    else if (header == mdp::worker::ident) {
        m_log.debug("zio::domo::Broker process worker");
        worker_process(sender, mmsg);
    }
    else {
        m_log.error("zio::domo::Broker invalid message from " + sender);
    }
}

void Broker::proc_heartbeat(time_unit_t heartbeat_at)
{
    auto now = now_ms();
    if (now < heartbeat_at) {
        return;
    }
    purge_workers();
    for (auto& wrk : m_waiting) {
        m_log.debug("zio::domo::Broker heartbeat to worker");
        zmq::multipart_t mmsg;
        mmsg.pushstr(mdp::worker::heartbeat);
        mmsg.pushstr(mdp::worker::ident);
        send(m_sock, mmsg, wrk->identity);
    }
}

void Broker::start()
{
    time_unit_t now = now_ms();
    time_unit_t heartbeat_at = now + m_hb_interval;

    zmq::poller_t<> poller;
    poller.add(m_sock, zmq::event_flags::pollin);
    while (! interrupted()) {
        time_unit_t timeout{0};
        if (heartbeat_at > now ) {
            timeout = heartbeat_at - now;
        }

        std::vector< zmq::poller_event<> > events(1);
        int rc = poller.wait_all(events, timeout);
        if (rc > 0) {           // got one
            proc_one();
        }
        proc_heartbeat(heartbeat_at);

        heartbeat_at += m_hb_interval;
        now = now_ms();
    }
}

void Broker::purge_workers()
{
    auto now = now_ms();
    // can't remove from the set while iterating, so make a temp
    std::vector<Worker*> dead;
    for (auto wrk : m_waiting) {
        if (wrk->expiry <= now) {
            dead.push_back(wrk); 
        }
    }
    for (auto wrk : dead) {
        m_log.debug("zio::domo::Broker deleting expired worker: " + wrk->identity);
        worker_delete(wrk,0);   // operates on m_waiting set
    }
}

Broker::Service* Broker::service_require(std::string name)
{
    Service* srv = m_services[name];
    if (!srv) {
        srv = new Service{name};
        m_services[name] = srv;
        m_log.debug("zio::domo::Broker registering new service: " + name);
    }
    return srv;
}

void Broker::service_internal(remote_identity_t rid, std::string service_name, zmq::multipart_t& mmsg)
{
    zmq::multipart_t response;

    if (service_name == "mmi.service") {
        std::string sn = mmsg.popstr();
        Service* srv = m_services[sn];
        if (srv and srv->nworkers) {
            response.pushstr("200");
        }
        else {
            response.pushstr("404");
        }
    }
    else {
        response.pushstr("501");
    }

    send(m_sock, response, rid);
}

void Broker::service_dispatch(Service* srv)
{
    purge_workers();
    while (srv->waiting.size() and srv->requests.size()) {

        std::list<Worker*>::iterator wrk_it = srv->waiting.begin();
        std::list<Worker*>::iterator next = wrk_it;
        for (++next; next != srv->waiting.end(); ++next) {
            if ((*next)->expiry > (*wrk_it)->expiry) {
                 wrk_it = next;
            }
        }
            
        zmq::multipart_t& mmsg = srv->requests.front();
        m_log.debug("zio::domo::Broker send work");        
        send(m_sock, mmsg, (*wrk_it)->identity);
        srv->requests.pop_front();
        m_waiting.erase(*wrk_it);
        srv->waiting.erase(wrk_it);
    }
}


Broker::Worker* Broker::worker_require(remote_identity_t identity)
{
    Worker* wrk = m_workers[identity];
    if (!wrk) {
        wrk = new Worker{identity};
        m_workers[identity] = wrk;
        m_log.debug("zio::domo::Broker registering new worker");
    }
    return wrk;
}

void Broker::worker_delete(Broker::Worker*& wrk, int disconnect)
{
    if (disconnect) {
        zmq::multipart_t mmsg;
        mmsg.pushstr(mdp::worker::disconnect);
        mmsg.pushstr(mdp::worker::ident);
        m_log.debug("zio::domo::Broker disconnect worker");
        send(m_sock, mmsg, wrk->identity);
    }
    if (wrk->service) {
        for (std::list<Worker*>::iterator it = wrk->service->waiting.begin();
                 it != wrk->service->waiting.end();) {
            if (*it == wrk) {
                it = wrk->service->waiting.erase(it);
            }
            else {
                ++it;
            }
        }
        --wrk->service->nworkers;
    }
    m_waiting.erase(wrk);
    m_workers.erase(wrk->identity);
    delete wrk;
    wrk=0;
}
// mmsg holds starting with 7/MDP Frame 2.
void Broker::worker_process(remote_identity_t sender, zmq::multipart_t& mmsg)
{
    assert(mmsg.size() >= 1);
    const std::string command = mmsg.popstr(); // 0x01, 0x02, ....
    bool worker_ready = (m_workers.find(sender) != m_workers.end());
    Worker* wrk = worker_require(sender);

    if (mdp::worker::ready == command) {
        if (worker_ready) {     // protocol error
            m_log.error("zio::domo::Broker protocol error (double ready) from: " + sender);
            worker_delete(wrk, 1);
            return;
        }
        if (sender.size() >= 4 && sender.find_first_of("mmi.") == 0) {
            m_log.error("zio::domo::Broker protocol error (worker mmi) from: " + sender);
            worker_delete(wrk, 1);
            return;
        }
        // Attach worker to service and mark as idle
        std::string service_name = mmsg.popstr();
        wrk->service = service_require(service_name);
        wrk->service->nworkers++;
        worker_waiting(wrk);
        return;
    }
    if (mdp::worker::reply == command) {
        if (!worker_ready) {
            worker_delete(wrk, 1);
            return;
        }
        remote_identity_t client_id = mmsg.popstr();
        mmsg.pop();
        mmsg.pushstr(wrk->service->name);
        mmsg.pushstr(mdp::client::ident);
        m_log.debug("zio::domo::Broker reply to client");
        send(m_sock, mmsg, client_id);
        worker_waiting(wrk);
        return;
    }
    if (mdp::worker::heartbeat == command) {
        if (!worker_ready) {
            worker_delete(wrk, 1);
            return;
        }
        wrk->expiry = now_ms() + m_hb_expiry;
        return;
    }
    if (mdp::worker::disconnect == command) {
        worker_delete(wrk, 0);
        return;
    }
    m_log.error("zio::domo::Broker invalid input message " + command);
}


void Broker::worker_waiting(Broker::Worker* wrk)
{
    m_waiting.insert(wrk);
    wrk->service->waiting.push_back(wrk);
    wrk->expiry = now_ms() + m_hb_expiry;
    
    service_dispatch(wrk->service);
}

void Broker::client_process(remote_identity_t client_id, zmq::multipart_t& mmsg)
{
    std::string service_name = mmsg.popstr(); // Client REQUEST Frame 2 
    Service* srv = service_require(service_name);
    if (service_name.size() >= 4 and service_name.find_first_of("mmi.") == 0) {
        service_internal(client_id, service_name, mmsg);
    }
    else {
        mmsg.pushmem(NULL,0);               // frame 4
        mmsg.pushstr(client_id);            // frame 3
        mmsg.pushstr(mdp::worker::request); // frame 2
        mmsg.pushstr(mdp::worker::ident);   // frame 1
        srv->requests.emplace_back(std::move(mmsg));
        service_dispatch(srv);
    }
}


// An actor function running a Broker.


void zio::domo::broker_actor(zmq::socket_t& pipe, std::string address, int socktype)
{
    console_log log;
    log.level = console_log::log_level::debug;

    zmq::context_t ctx;
    zmq::socket_t sock(ctx, socktype);
    sock.bind(address);

    Broker broker(sock, log);
    pipe.send(zmq::message_t{}, zmq::send_flags::none);

    // basically the guts of start() but we also poll on pipe as well as sock

    time_unit_t now = now_ms();
    time_unit_t hb_interval{HEARTBEAT_INTERVAL};
    time_unit_t heartbeat_at = now + hb_interval;

    zmq::poller_t<> poller;
    poller.add(pipe, zmq::event_flags::pollin);
    poller.add(sock, zmq::event_flags::pollin);

    while (! interrupted()) {
        time_unit_t timeout{0};
        if (heartbeat_at > now ) {
            timeout = heartbeat_at - now;
        }

        log.debug("broker actor wait");
        std::vector< zmq::poller_event<> > events(2);
        int nevents = poller.wait_all(events, timeout);
        for (int iev=0; iev < nevents; ++iev) {

            if (events[iev].socket == sock) {
                log.debug("broker actor sock hit");
                broker.proc_one();
            }

            if (events[iev].socket == pipe) {
                log.debug("broker actor pipe hit");
                zmq::message_t msg;
                auto res = events[0].socket.recv(msg, zmq::recv_flags::dontwait);
                assert(res);
                std::stringstream ss;
                ss << "msg: " << msg.size();
                log.debug("broker actor pipe " + ss.str());
                return;         // terminated
            }
        }
        if (!nevents) {
            log.debug("broker actor timeout");
        }
        broker.proc_heartbeat(heartbeat_at);

        heartbeat_at += hb_interval;
        now = now_ms();
    }

    zmq::message_t die;
    auto res = pipe.recv(die);
}

