/** 

    Simplified access to a ZeroMQ socket.

 */

#ifndef ZIO_SOCKET_HPP_SEEN
#define ZIO_SOCKET_HPP_SEEN

#include <czmq.h>
#include <vector>
#include <cstdint>

#include "zio/types.hpp"

namespace zio {

    /*!
      @brief thin wrapper around ZeroMQ socket
    */
    class Socket {
    public:
        typedef std::vector<std::uint8_t> msg_data_t;

        Socket(int stype);
        Socket(const Socket& ) = delete;
        Socket& operator=(const Socket&) = delete;
        ~Socket();

        /// Bind socket to address, return fully qualified endpoint.
        address_t bind(const address_t& address);

        /// Connect socket to address
        void connect(const address_t& address);

        /// A SUB needs to subscribe to something, if it expects to
        /// get messages.  Call this prior to any bind or connect.
        typedef std::string prefixmatch_t;
        void subscribe(const prefixmatch_t& sub = "");

        /// Send message out socket.
        void send(const msg_data_t& msg);

        /// Wrap ZMQ receiving, exposing the zmsg_t.  Timeout in msec.
        /// Return null msg if timeout occurs.
        msg_data_t recv(int timeout=-1);

        /// Return ZMQ socket type
        int type() { return zsock_type(m_sock); }

        /// Return true if there is at least one message waiting in
        /// the socket input queue.
        bool pollin();

        /// Return true if a send would not block
        bool pollout();

        // Access underlying ZeroMQ/CZMQ zsock_t.
        zsock_t* zsock() { return m_sock; }

    private:
        zsock_t* m_sock;
        zpoller_t* m_poller;
    };

}
#endif
