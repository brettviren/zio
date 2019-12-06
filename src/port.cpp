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
    m_binders.push_back(DirectBinder{m_sock, m_ctx.hostname});
}

void zio::Port::connect(const address_t& address)
{
    m_connect_addresses.push_back(address);
}

void zio::Port::connect(const nodename_t& node, const portname_t& port)
{
    m_connect_nodeports.push_back(std::make_pair(node,port));
}


zio::headerset_t zio::Port::do_binds()
{
    zio::headerset_t ret;
    for (auto& binder : m_binders) {
        auto address = binder();
        ret.push_back(make_address_header(m_name, address));
        m_bound.push_back(address);
    }
    return ret;
}

void zio::Port::online(zio::Peer& peer)
{
    if (m_online) { return; }
    m_online = true;

    for (const auto& nh : m_connect_nodeports) {
        zsys_debug("waiting for %s", nh.first.c_str());
        auto uuids = peer.waitfor(nh.first);
        assert(uuids.size());

        const std::string prefix = nh.second + "@";
        for (auto uuid : uuids) {
            auto pi = peer.peer_info(uuid);

            auto found = pi.match("Zio-Port", prefix);
            assert(found.size());
            for (const auto& val : found) {
                address_t addr = val.substr(prefix.size());
                zsock_connect(m_sock.zsock(), "%s", addr.c_str());
                m_connected.push_back(addr);
            }
        }
    }
    
    for (const auto& addr : m_connect_addresses) {
        zsock_connect(m_sock.zsock(), "%s", addr.c_str());
        m_connected.push_back(addr);
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


void zio::Port::send(level::MessageLevel lvl, const Format& payload)
{
    zmsg_t* msg = zmsg_new();
    zmsg_addstrf(msg, "ZIO%d%s%ld%ld", lvl,
                 payload.type(), m_ctx.origin, m_ctx.gf());
    zmsg_addmem(msg, payload.data(), payload.size());
    zmsg_send(&msg, m_sock.zsock());    
}
