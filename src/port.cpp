#include "zio/port.hpp"

#include <sstream>
#include <algorithm>
#include <string>

struct DirectBinder {
    zio::socket_t& sock;
    std::string address;
    std::string operator()() {
        sock.bind(address);
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
    zio::socket_t& sock;
    std::string hostname;
    int tcpportnum{0};
    std::string operator()() {
        std::string address = make_tcp_address(hostname, tcpportnum);
        sock.bind(address);
        return address;
    }
};

zio::Port::Port(const std::string& name, int stype, const std::string& hostname)
    : m_name(name), m_sock(m_ctx , stype), m_hostname(hostname), m_online(false)
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
    zsys_debug("[port %s] bind default", m_name.c_str());
    bind(m_hostname, 0);
}

void zio::Port::bind(const std::string& hostname, int port)
{
    zsys_debug("[port %s]: bind host/port: %s:%d",
               m_name.c_str(), hostname.c_str(), port);
    HostPortBinder binder{m_sock, hostname, 0};
    m_binders.push_back(binder);
}

void zio::Port::bind(const address_t& address)
{
    zsys_debug("[port %s]: bind address: %s",
               m_name.c_str(), address.c_str());
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
    if (zio::sock_type(m_sock) == ZMQ_SUB) 
        m_sock.setsockopt(ZMQ_SUBSCRIBE, prefix.c_str(), prefix.size());
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

    zsys_debug("DEBUG: binders: %d", m_binders.size());
        
    for (auto& binder : m_binders) {
        auto address = binder();
        ss << comma << address;
        comma = " ";
        m_bound.push_back(address);
    }
    std::string addresses = ss.str();
    set_header("address", addresses);
    set_header("socket", zio::sock_type_name(zio::sock_type(m_sock)));
    for (const auto& hh : m_headers) {
        zsys_debug("[port %s]: %s = %s",
                   m_name.c_str(), hh.first.c_str(), hh.second.c_str());
    }

    return m_headers;
}

void zio::Port::online(zio::Peer& peer)
{
    if (m_online) { return; }
    m_online = true;

    if (m_verbose) {
        zsys_debug("[port %s]: going online with %d(%d+%d) connects, %d binds",
                   m_name.c_str(), 
                   m_connect_nodeports.size()+m_connect_addresses.size(),
                   m_connect_nodeports.size(),
                   m_connect_addresses.size(),
                   m_binders.size());
    }

    for (const auto& addr : m_connect_addresses) {
        if (m_verbose) 
            zsys_debug("[port %s]: connect to %s",
                       m_name.c_str(), addr.c_str());
        m_sock.connect(addr);
        m_connected.push_back(addr);
    }

    for (const auto& nh : m_connect_nodeports) {
        if (m_verbose)
            zsys_debug("[port %s]: wait for %s",
                       m_name.c_str(), nh.first.c_str());
        auto uuids = peer.waitfor(nh.first);
        assert(uuids.size());
        if (m_verbose)
            zsys_debug("[port %s]: %d peers match %s",
                       m_name.c_str(), uuids.size(), nh.first.c_str());

        for (auto uuid : uuids) {
            auto pi = peer.peer_info(uuid);

            std::string maybe = pi.branch("zio.port." + nh.second)[".address"];
            if (maybe.empty()) {
                zsys_warning("[port %s]: found %s:%s (%s) lacking address header",
                             m_name.c_str(), nh.first.c_str(), nh.second.c_str(), uuid.c_str());
                continue;
            }
            std::stringstream ss(maybe);
            std::string addr;
            while(std::getline(ss, addr, ' ')) {
                if (addr.empty() or addr[0] == ' ') continue;
                if (m_verbose) 
                    zsys_debug("[port %s]: connect to %s:%s at %s",
                               m_name.c_str(),
                               nh.first.c_str(), nh.second.c_str(), addr.c_str());
                m_sock.connect(addr);
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
        m_sock.disconnect(addr);
    }

    for (const auto &addr : m_bound) {
        m_sock.unbind(addr);
    }
    m_connected.clear();
}

static bool needs_codec(int stype)
{
    return
        stype == ZMQ_SERVER ||
        stype == ZMQ_CLIENT ||
        stype == ZMQ_RADIO ||
        stype == ZMQ_DISH;
}


void zio::Port::send(zio::Message& msg)
{
    msg.set_coord(m_origin);
    if (needs_codec(zio::sock_type(m_sock))) {
        zio::message_t spmsg = msg.encode();
        m_sock.send(spmsg, zio::send_flags::none);
        return;
    }
    zio::multipart_t mpmsg = msg.toparts();
    mpmsg.send(m_sock);
}

bool zio::Port::recv(Message& msg, int timeout)
{
    zio::pollitem_t items[] = {{m_sock, 0, ZMQ_POLLIN, 0}};
    int item = zio::poll(&items[0], 1, timeout);
    if (!item) return false;

    if (needs_codec(zio::sock_type(m_sock))) {
        zio::message_t spmsg;
        auto res = m_sock.recv(spmsg);
        if (!res) return false;
        msg.decode(spmsg);
        return true;
    }

    zio::multipart_t mpmsg;
    bool ok = mpmsg.recv(m_sock);
    if (!ok) return false;
    msg.fromparts(mpmsg);

    return true;
}


