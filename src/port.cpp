#include "zio/port.hpp"
#include "zio/types.hpp"
#include <sstream>
#include <algorithm>
#include <string>

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


void zio::Port::set_header(const std::string& leafname, const std::string& value)
{
    std::string key = "zio.port." + m_name + "." + leafname;
    m_headers[key] = value;
}

zio::headerset_t zio::Port::do_binds()
{
    std::stringstream ss;
    std::string comma = "";

    for (auto& binder : m_binders) {
        auto address = binder();
        ss << comma << address;
        comma = " ";
        m_bound.push_back(address);
    }
    std::string addresses = ss.str();
    set_header("address", addresses);

    set_header("socket", m_sock.stype());

    return m_headers;

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

        for (auto uuid : uuids) {
            auto pi = peer.peer_info(uuid);

            std::string maybe_addresses = pi.branch("zio.port." + nh.second)[".address"];
            if (maybe_addresses.empty()) {
                zsys_warning("[port %s]: found %s:%s (%s) lacking address header",
                             m_name.c_str(), nh.first.c_str(), nh.second.c_str(), uuid.c_str());
                continue;
            }
            std::stringstream ss(maybe_addresses);
            std::string addr;
            while(std::getline(ss, addr, ' ')) {
                if (addr.empty() or addr[0] == ' ') continue;
                if (m_verbose) 
                    zsys_debug("[port %s]: connect to %s:%s at %s",
                               m_name.c_str(),
                               nh.first.c_str(), nh.second.c_str(), addr.c_str());
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



void zio::Port::send(zio::Message& msg)
{
    msg.set_coord(m_origin);
    auto data = msg.encode();
    // zsys_debug("LOG[%d]: %s (len:%ld)", lvl, log.c_str(), data.size());
    m_sock.send(data);
}

bool zio::Port::recv(Message& msg, int timeout)
{
    zio::Message::encoded_t dat = m_sock.recv(timeout);
    if (dat.empty()) {
        return false;
    }
    msg.decode(dat);
    return true;
}


