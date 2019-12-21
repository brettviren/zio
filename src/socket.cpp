#include "zio/socket.hpp"

zio::Socket::Socket(int stype)
{
    m_sock = zsock_new(stype);
    if (!m_sock) {
        throw std::runtime_error("failed to make socket");
    }
    m_poller = zpoller_new(m_sock, NULL);
}
zio::Socket::~Socket()
{
    zpoller_destroy(&m_poller);
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

void zio::Socket::send(const zio::Socket::msg_data_t& data)
{
    zmsg_t* msg = zmsg_new();
    zmsg_addmem(msg, data.data(), data.size());
    zmsg_send(&msg, m_sock);
}

zio::Socket::msg_data_t zio::Socket::recv(int timeout)
{
    if (timeout >= 0) {
        void* sock = zpoller_wait(m_poller, timeout);
        if (!sock) {
            return zio::Socket::msg_data_t();
        }
    }
    zmsg_t* msg = zmsg_recv(m_sock);
    if (!msg) {
        return zio::Socket::msg_data_t();
    }

    zframe_t* frame = zmsg_pop(msg);
    zio::Socket::msg_data_t ret(zframe_data(frame),
                                zframe_data(frame) + zframe_size(frame));
    zframe_destroy(&frame);
    zmsg_destroy(&msg);
    return ret;
}

bool zio::Socket::pollin()
{
    return zsock_events(m_sock) & ZMQ_POLLIN;
}

bool zio::Socket::pollout()
{
    return zsock_events(m_sock) & ZMQ_POLLOUT;
}
