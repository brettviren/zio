/*!

  Senders provide simplified and typed sending to a port.

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
        explicit TextSender(portptr_t port);
        void operator()(zio::level::MessageLevel lvl, const std::string& log);

    };
    
    struct JsonSender {
        Message msg;
        portptr_t port;
        JsonSender() {}
        explicit JsonSender(portptr_t port);
        void operator()(zio::level::MessageLevel lvl, const zio::json& met);
    };

}

#endif
