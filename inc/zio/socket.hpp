/** 

    Simplified access to a ZeroMQ socket.

 */

#ifndef ZIO_SOCKET_HPP_SEEN
#define ZIO_SOCKET_HPP_SEEN

#include "zio/types.hpp"
#include <czmq.h>
#include <memory>

namespace zio {

    // A non-copyable wrapper around ZeroMQ/CZMQ socket.  This shall
    // be used only from the thread in which it was created.
    class Socket {
    public:
        Socket(int stype);
        Socket(const Socket& ) = delete;
        Socket& operator=(const Socket&) = delete;
        ~Socket();

        // Bind socket to address, return fully qualified endpoint.
        address_t bind(const address_t& address);

        // Connect socket to address
        void connect(const address_t& address);

        zsock_t* zsock() { return m_sock; }

    private:
        zsock_t* m_sock;
    };

}

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

#endif
