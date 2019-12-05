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

#include <memory>

namespace zio {

    typedef std::shared_ptr<Port> portptr_t;

    class Node {
        nickname_t m_nick;
        origin_t m_origin;
        Peer* m_peer;
        std::unordered_map<std::string, portptr_t> m_ports;
    public:
        Node(nickname_t nick, origin_t origin);

        // create a port of type and name
        portptr_t port(const std::string& name, int stype);

        // return previously created port
        portptr_t port(const std::string& name);


        // bring node online
        void online();

        // bring node offline
        void offline();

    };
}

#endif
