#include "zio/node.hpp"

zio::Node::Node(nickname_t nick, origin_t origin)
    : m_nick(nick), m_origin(origin), m_peer(nullptr)
{
}


zio::portptr_t zio::Node::port(const std::string& name, int stype)
{
    zio::portptr_t ret = port(name);
    if (ret) { return ret; }
    ret = std::make_shared<Port>(name,stype);
    m_ports[name] = ret;
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


void zio::Node::online()
{
    if (m_peer) { return; }

    headerset_t headers;
    for (auto& np : m_ports) {
        headerset_t hs = np.second->do_binds();
        headers.insert(headers.end(), hs.begin(), hs.end());
    }
    m_peer = new Peer(m_nick, headers);
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
