
#ifndef ZIO_NODE_HPP_SEEN
#define ZIO_NODE_HPP_SEEN

#include "zio/port.hpp"

namespace zio {

    /*!
      @brief An identified vertex in a ported, directed graph

      Each port (a @ref zio::Port) is named uniquely within the node
      and has a ZeroMQ socket which may (simultaneously) bind and
      connect.

      Every bind is advertised for discovery by a @ref zio::Peer.  

      Each connect may be direct which completes immediately or
      indirect (given based on node/port names) which waits for peer
      discovery to resolve.

      Nodes start in "offline" state.  During that state, ports may be
      created.  The ports also start in "offline" state and while offline
      they may bind() or connect().  When a node is brought "online" it
      brings all ports "online" and it is then when any bind() or
      connect() and associated discovery are performed.  A node and its
      ports may be taken subsequently "offline" and the cycle repeated.

    */
    class Node {
        nickname_t m_nick;
        origin_t m_origin;

        std::string m_hostname;
        Peer* m_peer;
        std::unordered_map<std::string, portptr_t> m_ports;
        std::vector<std::string> m_portnames; // in order of creation.
        bool m_verbose{false};

    public:
        /// Create a node.
        Node(nickname_t nick="", origin_t origin=0,
             const std::string& hostname="");

        ~Node();

        /// Return a previously set node nickname
        nickname_t nick() const { return m_nick; }
        /// Return a previously set node origin 
        origin_t origin() const { return m_origin; }
        /// Return names of all ports in order of their creation.
        const std::vector<std::string>& portnames() const {
            return m_portnames;
        }

        /// Set the node nickname
        void set_nick(const nickname_t& nick) { m_nick = nick; }

        /// Set the node origin
        void set_origin(origin_t origin);

        /// Set verbose for underlying Zyre and internal debug messages
        void set_verbose(bool verbose = true);
        bool verbose() const { return m_verbose; }

        /// @brief Create a named port with the given socket type
        ///
        /// If port of given name exits, return it.
        portptr_t port(const std::string& name, int stype);

        /// Return a previously created port
        portptr_t port(const std::string& name);
        
        /// @brief Bring the node online.
        /// 
        /// The extra headers are merged with those provided by this nodes @ref zio::Peer.
        void online(const headerset_t& extra_headers = {});

        /// Bring the node offline.
        void offline();

    };
}

#endif
