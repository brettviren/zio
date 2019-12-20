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

zio::Port::Port(const std::string& name, int stype, const std::string& hostname)
    : m_name(name), m_sock(stype), m_hostname(hostname), m_online(false)
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
    bind(m_hostname, 0);
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





