#include "zio/socket.hpp"
#include "zio/exceptions.hpp"

zio::Socket::Socket(int stype)
{
    m_sock = zsock_new(stype);
    /// apparently, can't even check for this because
    /// zsock_new_checked() intercedes
    ///
    // if (!m_sock) {
    //     throw zio::socket_error::create("failed to make socket");
    // }
    m_poller = zpoller_new(m_sock, NULL);

    m_encoded = false;          
    if (stype == ZMQ_SERVER or
        stype == ZMQ_CLIENT or
        stype == ZMQ_RADIO or
        stype == ZMQ_DISH)
        m_encoded = true;
    // encode for single message part
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
        throw zio::socket_error::create(address.c_str());
    }
    return zsock_endpoint(m_sock);
}

void zio::Socket::connect(const address_t& address)
{
    int rc = zsock_connect(m_sock, "%s", address.c_str());
    if (rc < 0) {
        throw zio::socket_error::create(address.c_str());
    }
}

void zio::Socket::subscribe(const prefixmatch_t& sub)
{
    zsock_set_subscribe(m_sock, sub.c_str());
}

void zio::Socket::send(const zio::Socket::msg_data_t& data)
{
    zmsg_t* msg = NULL;
    if (m_encoded) {            // send encoded into one message part
        msg = zmsg_new();
        zmsg_addmem(msg, data.data(), data.size());
    }
    else {
        zframe_t* frame = zframe_new(data.data(), data.size());
        msg = zmsg_decode(frame);
        zframe_destroy(&frame);
    }
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
    
    zframe_t* frame = 0;
    if (m_encoded) {
        frame = zmsg_pop(msg);
    }
    else{
        frame = zmsg_encode(msg);
    }
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
