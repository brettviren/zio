#include "zio/port.hpp"
#include "zio/util.hpp"
#include "zio/logging.hpp"

#include <sstream>
#include <algorithm>
#include <string>

static std::string make_tcp_address(std::string hostname, int port)
{
    std::stringstream ss;
    ss << "tcp://" << hostname << ":";
    if (port)
        ss << port;
    else
        ss << "*";
    return ss.str();
}

struct DirectBinder
{
    zio::socket_t& sock;
    std::string address;
    std::string operator()()
    {
        sock.bind(address);
        zio::debug("DirectBinder {}", address);
        return address;
    }
};

struct HostPortBinder
{
    zmq::socket_t& sock;
    std::string hostname;
    int tcpportnum{0};
    std::string operator()()
    {
        if (tcpportnum != 0) {
            std::string address = make_tcp_address(hostname, tcpportnum);
            sock.bind(address);
            return address;
        }
        for (int port = 49152; port < 65535; ++port) {
            std::string address = make_tcp_address(hostname, port);
            try {
                sock.bind(address);
                return address;
            } catch (zio::error_t& e) {
                zio::debug("failed to bind({})", address);
                continue;
            }
        }
        throw std::runtime_error("exaused ephemeral ports");
    }
};

zio::Port::Port(const std::string& name, int stype, const std::string& hostname)
    : m_name(name)
    , m_sock(m_ctx, stype)
    , m_hostname(hostname)
    , m_online(false)
{
}

zio::Port::~Port()
{
    if (m_online) {  // fixme: add state checks throughout
        offline();
    }
}

void zio::Port::bind()
{
    zio::debug("[port {}] bind default", m_name);
    bind(m_hostname, 0);
}

void zio::Port::bind(const std::string& hostname, int port)
{
    zio::debug("[port {}] bind host/port: {}:{}", m_name, hostname, port);
    HostPortBinder binder{m_sock, hostname, 0};
    m_binders.push_back(binder);
}

void zio::Port::bind(const address_t& address)
{
    zio::debug("[port {}] bind address: {}", m_name, address);
    m_binders.push_back(DirectBinder{m_sock, address});
}

void zio::Port::connect(const address_t& address)
{
    m_connect_addresses.push_back(address);
}

void zio::Port::connect(const nodename_t& node, const portname_t& port)
{
    m_connect_nodeports.push_back(std::make_pair(node, port));
}

void zio::Port::subscribe(const std::string& prefix)
{
    if (zio::sock_type(m_sock) == ZMQ_SUB) {
        m_sock.set(zmq::sockopt::subscribe, prefix);
    }
}

void zio::Port::set_header(const std::string& leafname,
                           const std::string& value)
{
    std::string key = "zio.port." + m_name + "." + leafname;
    m_headers[key] = value;
}

zio::headerset_t zio::Port::do_binds()
{
    std::stringstream ss;
    std::string comma = "";

    // zio::debug("DEBUG: binders: {}", m_binders.size());

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
        zio::debug("[port {}] {} = {}", m_name, hh.first, hh.second);
    }

    return m_headers;
}

void zio::Port::online(zio::Peer& peer)
{
    if (m_online) { return; }
    m_online = true;

    zio::debug("[port {}] going online with {}({}+{}) connects, {} binds",
               m_name, m_connect_nodeports.size() + m_connect_addresses.size(),
               m_connect_nodeports.size(), m_connect_addresses.size(),
               m_binders.size());

    for (const auto& addr : m_connect_addresses) {
        zio::debug("[port {}] connect to {}", m_name, addr);
        m_sock.connect(addr);
        m_connected.push_back(addr);
    }

    for (const auto& nh : m_connect_nodeports) {
        zio::debug("[port {}] wait for {}", m_name, nh.first);
        auto uuids = peer.waitfor(nh.first);
        assert(uuids.size());
        zio::debug("[port {}] {} peers match {}", m_name, uuids.size(),
                   nh.first);

        for (auto uuid : uuids) {
            auto pi = peer.peer_info(uuid);

            std::string maybe = pi.branch("zio.port." + nh.second)[".address"];
            if (maybe.empty()) {
                zio::warn("[port {}] found {}:{} ({}) lacking address header",
                          m_name, nh.first, nh.second, uuid);
                continue;
            }
            std::stringstream ss(maybe);
            std::string addr;
            while (std::getline(ss, addr, ' ')) {
                if (addr.empty() or addr[0] == ' ') { continue; }
                zio::debug("[port {}] connect to {}:{} at {}", m_name, nh.first,
                           nh.second, addr);
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

    for (const auto& addr : m_connected) { m_sock.disconnect(addr); }

    for (const auto& addr : m_bound) { m_sock.unbind(addr); }
    m_connected.clear();
}

// static bool needs_codec(int stype)
// {
//     return
//         stype == ZMQ_SERVER ||
//         stype == ZMQ_CLIENT ||
//         stype == ZMQ_RADIO ||
//         stype == ZMQ_DISH;
// }

bool zio::Port::send(zio::Message& msg, timeout_t /*timeout*/)
{
    // zio::debug("[port {}] send {} #{} {}",
    //            m_name, msg.form(), msg.seqno(),
    //            zio::binstr(msg.remote_id()));
    msg.set_coord(m_origin);
    zio::multipart_t mmsg = msg.toparts();
    if (zio::is_serverish(m_sock)) {
        send_serverish(m_sock, mmsg, msg.remote_id());
        return true;
    }
    if (zio::is_clientish(m_sock)) {
        send_clientish(m_sock, mmsg);
        return true;
    }

    throw std::runtime_error("Port::send: unsupported socket type");
}

bool zio::Port::recv(Message& msg, timeout_t timeout)
{
    long tout = -1;
    if (timeout.has_value()) { tout = timeout.value().count(); }
    // zio::debug("[port {}] polling for {}", m_name, tout);
    zio::pollitem_t items[] = {{m_sock, 0, ZMQ_POLLIN, 0}};
    int item = zio::poll(&items[0], 1, tout);
    if (!item) return false;

    if (zio::is_serverish(m_sock)) {
        zio::multipart_t mmsg;
        remote_identity_t remid;
        recv_serverish(m_sock, mmsg, remid);
        msg.fromparts(mmsg);
        msg.set_remote_id(remid);
        return true;
    }
    if (zio::is_clientish(m_sock)) {
        zio::multipart_t mmsg;
        recv_clientish(m_sock, mmsg);
        msg.fromparts(mmsg);
        return true;
    }
    throw std::runtime_error("Port::recv: unsupported socket type");
}
