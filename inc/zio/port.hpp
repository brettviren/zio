#ifndef ZIO_PORT_HPP_SEEN
#define ZIO_PORT_HPP_SEEN

#include "zio/socket.hpp"
#include "zio/peer.hpp"
#include <map>


namespace zio {

    typedef std::string nodename_t;
    typedef std::string portname_t;



    class Port {
        const std::string m_name;
        Socket m_sock;          // makes ports noncopyable
        const std::string m_hostname;
        bool m_online;

        // functions which perform a bind() and return associated header
        typedef std::function<address_t()> binder_t;
        std::vector<binder_t> m_binders;

        std::vector<address_t> m_connect_addresses, m_connected, m_bound;
        std::vector< std::pair<nodename_t, portname_t> > m_connect_nodeports;
        


    public:
        Port(const std::string& name, int stype, const std::string& my_hostname = "");
        ~Port();

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


        // The rest are for the owning Node to call

        // Do any requested binds, return all corresponding port headers.
        headerset_t do_binds();

        // Go online, making any connections, using peer to resolve if needed.
        void online(Peer& peer);



        // Disconnect, unbind.
        void offline();

        // void send();
        // void recv();


    };

}
#endif
