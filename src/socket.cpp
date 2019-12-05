#include "zio/socket.hpp"

zio::Socket::Socket(int stype)
{
    m_sock = zsock_new(stype);
    if (!m_sock) {
        throw std::runtime_error("failed to make socket");
    }
}
zio::Socket::~Socket()
{
    zsock_destroy(&m_sock);
}

zio::address_t zio::Socket::bind(const address_t& address)
{
    int port = zsock_bind(m_sock, "%s", address.c_str());
    if (port < 0) {
        std::string s = "failed to bind to " + address;
        throw std::runtime_error(s);
    }
    return zsock_endpoint(m_sock);
}

void zio::Socket::connect(const address_t& address)
{
    int rc = zsock_connect(m_sock, "%s", address.c_str());
    if (rc < 0) {
        std::string s = "failed to connect to " + address;
        throw std::runtime_error(s);
    }
}

void zio::Socket::subscribe(const prefixmatch_t& sub)
{
    zsock_set_subscribe(m_sock, sub.c_str());
}

