#ifndef ZIO_PORT_HPP_SEEN
#define ZIO_PORT_HPP_SEEN

#include "zio/socket.hpp"
#include "zio/peer.hpp"
#include "zio/format.hpp"
#include "json.hpp"
#include <map>


namespace zio {

    typedef std::string nodename_t;
    typedef std::string portname_t;



    struct TimeGranule {
        granule_t operator()() {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
    };

    struct PortCtx {
        std::string hostname;
        origin_t origin;
        granule_func_t gf{TimeGranule()};
    };

    class Port {
        const std::string m_name;
        Socket m_sock;          // makes ports noncopyable
        PortCtx m_ctx;
        bool m_online;

        // functions which perform a bind() and return associated header
        typedef std::function<address_t()> binder_t;
        std::vector<binder_t> m_binders;

        std::vector<address_t> m_connect_addresses, m_connected, m_bound;
        std::vector< std::pair<nodename_t, portname_t> > m_connect_nodeports;
        


    public:
        Port(const std::string& name, int stype, const PortCtx& ctx);
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


        // message passing via the port

        void send(level::MessageLevel lvl, const Format& payload);

        // void recv();

    };

    // Ports can not be copied because of the Socket but they
    // typically must be shared.
    typedef std::shared_ptr<Port> portptr_t;


}
#endif
