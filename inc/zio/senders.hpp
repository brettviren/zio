/** Senders provide functional objects to send data simply.

*/

#ifndef ZIO_SENDERS_HPP_SEEN
#define ZIO_SENDERS_HPP_SEEN

#include "zio/message.hpp"
#include "zio/port.hpp"
#include "zio/node.hpp"

namespace zio {

    struct TextSender {
        Message msg;
        portptr_t port;
        TextSender() {}
        TextSender(Node& node, std::string portname, int socket_type = ZMQ_PUB);
        void operator()(zio::level::MessageLevel lvl, const std::string& log);

    };
    
    struct JsonSender {
        Message msg;
        portptr_t port;
        JsonSender() {}
        JsonSender(Node& node, std::string portname, int socket_type = ZMQ_PUB);
        void operator()(zio::level::MessageLevel lvl, const zio::json& met);
    };

}

#endif
