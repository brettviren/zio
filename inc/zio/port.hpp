#ifndef ZIO_PORT_HPP_SEEN
#define ZIO_PORT_HPP_SEEN

#include "zio/peer.hpp"
#include "zio/message.hpp"
#include "zio/util.hpp"

#include <memory>
#include <map>


namespace zio {

    /*! 
      @brief A port holds a socket in the context of a @ref node.

      A port provides an identity (name) for the socket in the context
      of a node to the network (via a peer) and to the application
      (via a method).

      It provides direct and discovery-based bind() and connect().

      Finally, using its send() and recv() their messages will adhere
      to the ZIO conventions.

    */
    class Port {
    public:
        typedef std::string address_t;
        typedef std::string nodename_t;
        typedef std::string portname_t;

        /// @brief Create a port of given name and socket type.
        ///
        /// The hostname sets the default for ephemeral binds.
        ///
        /// A port is typically only constructed via a @ref zio::Node.
        Port(const std::string& name, int stype,
             const std::string& hostname = "127.0.0.1");
        ~Port();

        /// Access the owning node's origin.
        void set_origin(origin_t origin) { m_origin = origin; }

        void set_verbose(bool verbose = true) { m_verbose = verbose; }

        /// Access this port's name.
        const std::string& name() const { return m_name; }

        /// @brief Request a default bind
        ///
        /// This is for application to call.
        void bind();

        /// @brief Request a bind to a specific TCP/IP host and port.
        ///
        /// TCP port number 0 implies to pick some random, unused port.
        ///
        /// This is for application to call.
        void bind(const std::string& hostname, int tcpportnum);

        /// @brief Request bind to fully qualified ZeroMQ address string.
        ///
        /// This is for application to call.
        void bind(const address_t& address);

        /// @brief Request connect to fully qualified ZeroMQ address string.
        ///
        /// This is for application to call.
        void connect(const address_t& address);

        /// @brief Request connect to abstract node/port names.
        ///
        /// This will resolve to a direct address by the @ref zio::Peer
        ///
        /// This is for application to call.
        void connect(const nodename_t& node, const portname_t& port);

        /// @brief Subscribe to a PUB topic
        ///
        /// This is only meaningful when the underlying socket is a
        /// SUB and in this case at least one subscription is required
        /// if there shall be any expectation of the app getting
        /// messages.
        ///
        /// This is for application to call.
        void subscribe(const std::string& prefix = "");

        /// @brief Set an extra port header
        ///
        /// The header is of the form:
        ///     zio.port.<portname>.<leafname> = <value>
        void set_header(const std::string& leafname, const std::string& value);

        /// @brief Perform any requested binds.
        ///
        /// The corresponding Zyre headers for any ports that bind are
        /// returned and should be given to the peer to announce.
        ///
        /// This method is intended for the @ref zio::Node to call.
        headerset_t do_binds();

        /// @brief Make any previously requested connections.
        ///
        /// The peer will be used to resolve any abstract addresses.
        ///
        /// This method is intended for the @ref zio::Node to call
        void online(Peer& peer);

        /// @brief Disconnect and unbind.
        ///
        /// This method is intended for the @ref zio::Node to call
        void offline();

        /// @brief Send a message.
        ///
        /// The @ref zio::Message is modified to set its coordinates.
        bool send(Message& msg, timeout_t timeout={});

        /// Recieve a message, return false if timeout occurred.
        bool recv(Message& msg, timeout_t timeout={});

        /// @brief Access the underlying cppzmq socket.
        ///
        /// This access is generally not recomended.
        zio::socket_t& socket() { return m_sock; }

    private:
        const std::string m_name;
        zio::context_t m_ctx;
        zio::socket_t m_sock;
        std::string m_hostname;
        bool m_online;
        std::map<std::string, std::string> m_headers;
        origin_t m_origin;

        // functions which perform a bind() and return associated header
        typedef std::function<address_t()> binder_t;
        std::vector<binder_t> m_binders;

        std::vector<address_t> m_connect_addresses, m_connected, m_bound;
        std::vector< std::pair<nodename_t, portname_t> > m_connect_nodeports;
        
        bool m_verbose{false};
    };

    /// The context can't be copied and ports like to be shared.
    typedef std::shared_ptr<Port> portptr_t;

}
#endif
