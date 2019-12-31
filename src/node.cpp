#include "zio/node.hpp"

static
std::string get_hostname()
{
    zactor_t *beacon = zactor_new (zbeacon, NULL);
    assert (beacon);
    zsock_send (beacon, "si", "CONFIGURE", 31415);
    char *tmp = zstr_recv (beacon);
    std::string ret = tmp;
    zstr_free (&tmp);
    zactor_destroy (&beacon);    
    return ret;
}



zio::Node::Node(nickname_t nick, origin_t origin,
                const std::string& hostname)
    : m_nick(nick)
    , m_origin(origin)
    , m_hostname(hostname)
    , m_peer(nullptr)
{
    if (m_hostname.empty()) {
        m_hostname = get_hostname();
    }
}

zio::Node::~Node()
{
    offline();
    m_ports.clear();
}

zio::portptr_t zio::Node::port(const std::string& name, int stype)
{
    zio::portptr_t ret = port(name);
    if (ret) { return ret; }
    ret = std::make_shared<Port>(name, stype, m_hostname);
    ret->set_origin(m_origin);
    ret->set_verbose(m_verbose);
    m_ports[name] = ret;
    m_portnames.push_back(name);
    return ret;
}

zio::portptr_t zio::Node::port(const std::string& name)
{
    auto it = m_ports.find(name);
    if (it == m_ports.end()) {
        return nullptr;
    }
    return it->second;    
}


void zio::Node::online(const headerset_t& extra_headers)
{
    if (m_peer) { return; }

    headerset_t headers = extra_headers;
    for (auto& np : m_ports) {
        headerset_t hs = np.second->do_binds();
        headers.insert(hs.begin(), hs.end());
    }
    if (m_verbose) {
        zsys_debug("[node %s] going online with:", m_nick.c_str());
        for (const auto& hh : headers) {
            zsys_debug("\t%s = %s", hh.first.c_str(), hh.second.c_str());
        }
    }
    m_peer = new Peer(m_nick, headers, m_verbose);
    for (auto& np : m_ports) {
        np.second->online(*m_peer);
    }
}

void zio::Node::offline()
{
    if (!m_peer) { return ; }
    for (auto& np : m_ports) {
        np.second->offline();
    }
    delete m_peer;
    m_peer = nullptr;
}


void zio::Node::set_origin(origin_t origin)
{
    m_origin = origin;
    for (auto& np : m_ports) {
        np.second->set_origin(origin);
    }
}
void zio::Node::set_verbose(bool verbose)
{
    m_verbose = verbose;
    if (m_peer) 
        m_peer->set_verbose(verbose);
}
