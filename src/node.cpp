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
                const std::string& hostname, granule_func_t gf)
    : m_nick(nick), m_origin(origin), m_defgf(gf),
      m_hostname(hostname), m_peer(nullptr)
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

zio::portptr_t zio::Node::port(const std::string& name, int stype,
                               granule_func_t gf)
{
    zio::portptr_t ret = port(name);
    if (ret) { return ret; }
    ret = std::make_shared<Port>(name, stype,
                                 PortCtx{m_hostname, m_origin, gf});
    ret->set_verbose(m_verbose);
    m_ports[name] = ret;
    return ret;
}

zio::portptr_t zio::Node::port(const std::string& name, int stype)
{
    return port(name, stype, m_defgf);
}

zio::portptr_t zio::Node::port(const std::string& name)
{
    auto it = m_ports.find(name);
    if (it == m_ports.end()) {
        return nullptr;
    }
    return it->second;    
}


void zio::Node::online()
{
    if (m_peer) { return; }

    headerset_t headers;
    for (auto& np : m_ports) {
        headerset_t hs = np.second->do_binds();
        headers.insert(headers.end(), hs.begin(), hs.end());
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
