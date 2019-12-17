#include "zio/port.hpp"
#include "zio/types.hpp"
#include <sstream>

static
zio::header_t make_address_header(const std::string& portname, const std::string& address)
{
    const std::string port_header_key = "Zio-Port";
    return zio::header_t(port_header_key, portname + "@" + address);
    // fixme: above bakes in a convention of header:
    // Zio-Port: <portname>@<address>
    // Concentrate convention into some general header utils
}

struct DirectBinder {
    zio::Socket& sock;
    std::string address;
    zio::address_t operator()() {
        int port = zsock_bind(sock.zsock(), "%s", address.c_str());
        if (port < 0) {
            std::string msg = "failed to bind to " + address;
            throw std::runtime_error(msg);
        }
        return address;
    }
};

static
std::string make_tcp_address(std::string hostname, int port)
{
    std::stringstream ss;
    ss << "tcp://" << hostname << ":";
    if (port)
        ss << port;
    else
        ss << "*";
    return ss.str();
}

struct HostPortBinder {
    zio::Socket& sock;
    std::string hostname;
    int tcpportnum{0};
    zio::address_t operator()() {
        zio::address_t address = make_tcp_address(hostname, tcpportnum);
        tcpportnum = zsock_bind(sock.zsock(), "%s", address.c_str());
        if (tcpportnum < 0) {
            std::string msg = "failed to bind to " + address;
            throw std::runtime_error(msg);
        }
        return make_tcp_address(hostname, tcpportnum);
    }
};

zio::Port::Port(const std::string& name, int stype, const PortCtx& ctx)
    : m_name(name), m_sock(stype), m_ctx(ctx), m_online(false)
{
}

zio::Port::~Port()
{
    if (m_online) {             // fixme: add state checks throughout
        offline();
    }
}


void zio::Port::bind()
{
    bind(m_ctx.hostname, 0);
}

void zio::Port::bind(const std::string& hostname, int port)
{
    HostPortBinder binder{m_sock, hostname, 0};
    m_binders.push_back(binder);
}

void zio::Port::bind(const address_t& address)
{
    m_binders.push_back(DirectBinder{m_sock, address});
}

void zio::Port::connect(const address_t& address)
{
    m_connect_addresses.push_back(address);
}

void zio::Port::connect(const nodename_t& node, const portname_t& port)
{
    m_connect_nodeports.push_back(std::make_pair(node,port));
}

void zio::Port::subscribe(const std::string& prefix)
{
    m_sock.subscribe(prefix);
}


zio::headerset_t zio::Port::do_binds()
{
    zio::headerset_t ret;
    for (auto& binder : m_binders) {
        auto address = binder();
        auto hdr = make_address_header(m_name, address);
        ret.push_back(hdr);
        m_bound.push_back(address);
        if (m_verbose)
            zsys_debug("[port %s]: bind to %s = %s",
                       m_name.c_str(), hdr.first.c_str(), hdr.second.c_str());
    }
    return ret;
}

void zio::Port::online(zio::Peer& peer)
{
    if (m_online) { return; }
    m_online = true;

    if (m_verbose) {
        zsys_debug("[port %s]: going online with %d node ports, %d direct and %d indirect addresses",
                   m_name.c_str(), m_connect_nodeports.size(),
                   m_connect_addresses.size(), m_connect_nodeports.size());
    }

    for (const auto& addr : m_connect_addresses) {
        if (m_verbose) 
            zsys_debug("[port %s]: connect to %s",
                       m_name.c_str(), addr.c_str());
        zsock_connect(m_sock.zsock(), "%s", addr.c_str());
        m_connected.push_back(addr);
    }

    for (const auto& nh : m_connect_nodeports) {
        if (m_verbose)
            zsys_debug("[port %s]: wait for %s", m_name.c_str(), nh.first.c_str());
        auto uuids = peer.waitfor(nh.first);
        assert(uuids.size());

        const std::string prefix = nh.second + "@";
        for (auto uuid : uuids) {
            auto pi = peer.peer_info(uuid);

            if (m_verbose) {
                zsys_debug("[port %s]: peer info for %s [%s]",
                           m_name.c_str(), pi.nick.c_str(), uuid.c_str());
                for (const auto& hh : pi.headers) 
                    zsys_debug("\t%s = %s", hh.first.c_str(), hh.second.c_str());
            }

            auto found = pi.match("Zio-Port", prefix);
            assert(found.size());
            for (const auto& val : found) {
                address_t addr = val.substr(prefix.size());
                if (m_verbose) 
                    zsys_debug("[port %s]: connect to %s at %s",
                               m_name.c_str(),
                               nh.second.c_str(), addr.c_str());
                zsock_connect(m_sock.zsock(), "%s", addr.c_str());
                m_connected.push_back(addr);
            }
        }
    }
    
}


void zio::Port::offline()
{
    if (!m_online) return;
    m_online = false;

    for (const auto& addr : m_connected) {
        zsock_disconnect(m_sock.zsock(), "%s", addr.c_str());
    }

    for (const auto &addr : m_bound) {
        zsock_unbind(m_sock.zsock(), "%s", addr.c_str());
    }
    m_connected.clear();
}


void zio::Port::send(level::MessageLevel lvl, const std::string& format,
                     const byte_array_t& buf,
                     const std::string& label)
{
    if (m_verbose)
        zsys_debug("[port %s]: send ZIO%d%4s%s",
                   m_name.c_str(), lvl, format.c_str(), label.c_str());
                   

    zmsg_t* msg = zmsg_new();
    zmsg_addstrf(msg, "ZIO%d%4s%s", lvl, format.c_str(), label.c_str());
    const int ncoords = 3;
    uint64_t coords[ncoords] = {m_ctx.origin, m_ctx.gf(), m_seqno++};
    zmsg_addmem(msg, coords, ncoords*sizeof(uint64_t));
    zmsg_addmem(msg, buf.data(), buf.size());
    zmsg_send(&msg, m_sock.zsock());    
    if (m_verbose)
        zsys_debug("[port %s]: send done", m_name.c_str());

}

int zio::Port::recv(Header& header, byte_array_t& payload)
{
    std::vector<byte_array_t> payloads;
    int rc = recv(header, payloads);
    if (rc < 0) {
        return rc;
    }
    if (payloads.size() != 1) {
        return -3;
    }
    payload = payloads[0];
    return 0;
}
    
int zio::Port::recv(Header& header, std::vector<byte_array_t>& payloads)
{
    if (m_verbose)
        zsys_debug("[port %s]: receving", m_name.c_str());
    zmsg_t* msg = zmsg_recv(m_sock.zsock());

    // prefix header
    if (zmsg_size(msg) > 0) {
        char* h1 = zmsg_popstr(msg);
        header.level = h1[3] - '0';
        std::string h1s = h1;
        header.format = h1s.substr(4,4);
        header.label = h1s.substr(8);
        free (h1);
    }
    else {
        zmsg_destroy(&msg);
        return -1;
    }

    const size_t wantsize = 3*sizeof(uint64_t);

    // coordinate header
    if (zmsg_size(msg) > 0) {
        zframe_t* frame = zmsg_pop(msg);
        if (zframe_size(frame) != wantsize) {
            if (m_verbose)
                zsys_warning("[port %s]: wrong coordinate frame size: %ld (expect %ld)",
                             m_name.c_str(), zframe_size(frame), wantsize);
                             
            zframe_destroy(&frame);
            zmsg_destroy(&msg);
            return -2;
        }
        uint64_t* ogs = (uint64_t*)zframe_data(frame);
        header.origin = ogs[0];
        header.granule = ogs[1];
        header.seqno = ogs[2];
        zframe_destroy(&frame);
    }
    else {
        zmsg_destroy(&msg);
        return -2;
    }

    // payloads
    while (zmsg_size(msg)) {
        zframe_t* frame = zmsg_pop(msg);
        std::uint8_t* b = zframe_data(frame);
        size_t s = zframe_size(frame);
        payloads.emplace_back(b, b+s);
        zframe_destroy(&frame);
    }
    // This method does not handle multiple payloads
    zmsg_destroy(&msg);
    return 0;
}
