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

namespace zio {

    class Node {
        nickname_t m_nick;
        origin_t m_origin;
        granule_func_t m_defgf;
        
        std::string m_hostname;
        Peer* m_peer;
        std::unordered_map<std::string, portptr_t> m_ports;
        bool m_verbose{false};
    public:
        Node(nickname_t nick="", origin_t origin=0,
             const std::string& hostname="",
             granule_func_t gf = TimeGranule());
        ~Node();

        void set_nick(const nickname_t& nick) { m_nick = nick; }
        void set_origin(origin_t origin) { m_origin = origin; }

        // set verbose
        void set_verbose(bool verbose = true) { m_verbose = verbose; }

        // Create a port of type and name using default granule function.
        portptr_t port(const std::string& name, int stype);
        // Use a unique granule function with the prot.
        portptr_t port(const std::string& name, int stype,
                       granule_func_t gf);

        // return previously created port
        portptr_t port(const std::string& name);


        // Bring node online using a zio::Peer with auto-generated
        // headers and any extra ones.
        void online(const headerset_t& extra_headers = {});

        // bring node offline
        void offline();

    };
}

#endif
