/**

   A zio node is an identified vertex in a ported, directed graph.

   Each port is named uniquely within the node and has a ZeroMQ socket
   which may (simultaneously) bind and connect.

   A bind is advertised for discovery.  

   A connect may be direct which completes immeidately or may be
   indirect which waits for peer discovery.

   Nodes start in "offline" state.  During that state, ports may be
   created.  The ports also start in "offline" state and while offline
   they may bind() or connect().  When a node is brought "online" it
   brings all ports "online" and it is then when any bind() or
   connect() and associated discovery are performed.  A node and its
   ports may be taken subsequently "offline" and the cycle repeated.

 */

#ifndef ZIO_NODE_HPP_SEEN
#define ZIO_NODE_HPP_SEEN

#include "zio/types.hpp"
#include "zio/port.hpp"
#include "zio/outbox.hpp"

namespace zio {

    class Node {
        nickname_t m_nick;
        origin_t m_origin;

        std::string m_hostname;
        Peer* m_peer;
        std::unordered_map<std::string, portptr_t> m_ports;
        std::vector<std::string> m_portnames; // in order of creation.
        bool m_verbose{false};
    public:
        Node(nickname_t nick="", origin_t origin=0,
             const std::string& hostname="");
        ~Node();

        nickname_t nick() const { return m_nick; }
        origin_t origin() const { return m_origin; }
        const std::vector<std::string>& portnames() const {
            return m_portnames;
        }

        void set_nick(const nickname_t& nick) { m_nick = nick; }
        void set_origin(origin_t origin) { m_origin = origin; }

        // set verbose
        void set_verbose(bool verbose = true) { m_verbose = verbose; }
        bool verbose() const { return m_verbose; }

        // Create a port of type and name using default granule function.
        portptr_t port(const std::string& name, int stype);

        // return previously created port
        portptr_t port(const std::string& name);

        // Return a logger.  Create if not yet created.
        Logger logger(const std::string& name, int stype = ZMQ_PUB);

        // Return a metric.  Create if not yet created.
        Metric metric(const std::string& name, int stype = ZMQ_PUB);

        // Bring node online using a zio::Peer with auto-generated
        // headers and any extra ones.
        void online(const headerset_t& extra_headers = {});

        // bring node offline
        void offline();

    };
}

#endif
