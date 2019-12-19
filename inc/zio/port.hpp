#ifndef ZIO_PORT_HPP_SEEN
#define ZIO_PORT_HPP_SEEN

#include "zio/socket.hpp"
#include "zio/peer.hpp"
#include "zio/format.hpp"

#include <memory>
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
        
        uint64_t m_seqno{0};
        bool m_verbose{false};

    public:
        Port(const std::string& name, int stype, const PortCtx& ctx);
        ~Port();

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
        void subscribe(const std::string& prefix);

        // The rest are for the owning Node to call

        // Do any requested binds, return all corresponding port headers.
        headerset_t do_binds();

        // Go online, making any connections, using peer to resolve if needed.
        void online(Peer& peer);

        // Disconnect, unbind.
        void offline();


        // Send with level and a single payload and optional label.
        // This forces policy on the granule, origin and seqno.
        void send(level::MessageLevel lvl, const std::string& format,
                  const byte_array_t& payload,
                  const std::string& label = "");

        // Send with full user provided header, no policy.
        void send(const Header& header, const byte_array_t& payload);

        // Do a blocking recv() or timeout after given msec.  Return
        // zero on success, -1 if timeout, -2 if message parse fails,
        // -3 if multiple payload arrays available but only one requested.
        int recv(Header& header, byte_array_t& payload, int timeout = -1);
        int recv(Header& header, std::vector<byte_array_t>& payloads, int timeout = -1);

        // Access wrapped socket.
        Socket& socket();
    };

    // Ports can not be copied because of the Socket but they
    // typically must be shared.
    typedef std::shared_ptr<Port> portptr_t;


}
#endif
