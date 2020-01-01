#ifndef ZIO_PORT_HPP_SEEN
#define ZIO_PORT_HPP_SEEN

#include "zio/peer.hpp"
#include "zio/message.hpp"
#include "zio/interned.hpp"

#include <memory>
#include <map>


namespace zio {

    /*! 
      @brief a port holds a socket in the context of a node

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

        Port(const std::string& name, int stype,
             const std::string& hostname = "127.0.0.1");
        ~Port();

        // The owning node's origin.
        void set_origin(origin_t origin) { m_origin = origin; }

        void set_verbose(bool verbose = true) { m_verbose = verbose; }

        const std::string& name() const { return m_name; }

        // Register bind/connect requests

        // bind to ephermeral port on interface associated with
        // hostname
        void bind();

        // bind to specific TCP/IP host and port.  TCP port number 0
        // implies ephermeral.
        void bind(const std::string& hostname, int tcpportnum);

        // Bind to fully qualified ZeroMQ address.
        void bind(const address_t& address);

        // Directly connect to fully qualified address
        void connect(const address_t& address);

        // Indirectly connect to a named node port
        void connect(const nodename_t& node, const portname_t& port);

        // When this is a SUB then at least one subscription is
        // required if there shall be any expectation of messages.
        void subscribe(const std::string& prefix = "");

        // Set header "zio.port.<portname>.<leafname> = <value>
        void set_header(const std::string& leafname, const std::string& value);

        /// For node.  Do any requested binds, return all
        /// corresponding port headers.
        headerset_t do_binds();

        /// For node.  Go online, making any connections, using peer
        /// to resolve if needed.
        void online(Peer& peer);

        /// For node.  Disconnect, unbind.
        void offline();

        /// Send a message.  Note, msg is modified to set coords.
        void send(Message& msg);

        /// Recieve a message, return false if timeout
        bool recv(Message& msg, int timeout=-1);

        // Access the underlying cppzmq socket.
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

    // The context can't be copied and ports like to be shared.
    typedef std::shared_ptr<Port> portptr_t;

}
#endif
