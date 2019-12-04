#ifndef ZIO_CONSUMER_HPP_SEEN
#define ZIO_CONSUMER_HPP_SEEN

#include "zio/socket.hpp"

namespace zio {

    class Consumer {
        std::shared_ptr<Socket> m_sock;
    public:
        Consumer(int stype);
        Consumer(std::shared_ptr<Socket>);

    };

}

#endif
