#ifndef ZIO_DOMO_BROKER_HPP_SEEN
#define ZIO_DOMO_BROKER_HPP_SEEN

#include "zio/util.hpp"
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <list>
#include <functional>

namespace zio {
namespace domo {


    /*! The ZIO domo broker class */

    class Broker {
    public:

        /// Create a broker with a ROUTER or SERVER socket already
        /// bound.  Caller must keep socket, eg to mix with others in
        /// an actor's poller.
        Broker(zio::socket_t& sock);
        ~Broker();

        /// Begin brokering (run forever).  The guts of this method
        /// need to be reimplemented if proc_*() are called instead.
        void start();

        /// Process one input on socket
        void proc_one();

        /// Do heartbeat processing given next heatbeat time. 
        void proc_heartbeat(time_unit_t heartbeat_at);

    private:

        std::function<remote_identity_t(zio::socket_t& server_socket,
                                        zio::multipart_t& mmsg)> recv;
        std::function<void(zio::socket_t& server_socket,
                           zio::multipart_t& mmsg, remote_identity_t rid)> send;

        struct Service;

        // This is a proxy for the remote worker
        struct Worker {
            // The identity of a worker.
            remote_identity_t identity; 

            // The owner, if known.
            Service* service{nullptr};
            // Expire the worker at this time, heartbeat refreshes.
            time_unit_t expiry{0};
        };

        // This collects workers for a given service
        struct Service {

            // Service name, that is the "thing" that its workers know how to do.
            std::string name;

            // List of client requests for this service.  Each holds a
            // full 7/MDP message starting with Frame 1.
            std::deque<zio::multipart_t> requests;

            // List of waiting workers.
            std::list<Worker*> waiting;

            // How many workers the service has
            size_t nworkers{0};

            ~Service ();
        };

    private:
        void purge_workers();
        Service* service_require(std::string name);
        void service_dispatch(Service* srv);
        void service_internal(remote_identity_t rid, std::string service_name,
                              zio::multipart_t& mmsg);

        Worker* worker_require(remote_identity_t identity);
        void worker_delete(Worker*& wrk, int disconnect);

        void worker_process(remote_identity_t sender, zio::multipart_t& mmsg);
        void worker_waiting(Worker* wkr);

        void client_process(remote_identity_t client_id, zio::multipart_t& mmsg);

    private:

        zio::socket_t& m_sock;

        // fixme: make configurable
        time_unit_t m_hb_interval{HEARTBEAT_INTERVAL};
        time_unit_t m_hb_expiry{HEARTBEAT_EXPIRY};

        std::unordered_map<remote_identity_t, Service*> m_services;
        std::unordered_map<remote_identity_t, Worker*> m_workers;
        std::unordered_set<Worker*> m_waiting;
    };


    /*! The launch and forget ZIO domo broker actor function.
     *
     * The broker is in the form of a function intended to be called
     * from a zio::zactor_t.
     *
     * The socktype should be either SERVER or ROUTER.
     * If ROUTER, the broker will act as a 7/MDP v0.1 broker.
     * Else it will act as a GDP broker.
     *
     * The actor protocol supports the commands:
     * - (BIND, address) :: bind broker socket to address
     * - (START) :: enter main brokering loop
     */
    void broker_actor(zio::socket_t& link, std::string address, int socktype);


}
}
#endif
