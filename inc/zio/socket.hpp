/** 

    Simplified access to a ZeroMQ socket.

 */

#ifndef ZIO_SOCKET_HPP_SEEN
#define ZIO_SOCKET_HPP_SEEN

#include "zio/types.hpp"
#include <czmq.h>

namespace zio {

    /// Socket provides a non-copyable wrapper around ZeroMQ/CZMQ
    /// socket.  This shall be used only from the thread in which it
    /// was created.
    class Socket {
    public:
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
        void subscribe(const prefixmatch_t& sub = "");

        zsock_t* zsock() { return m_sock; }

    private:
        zsock_t* m_sock;
    };

}
#endif
