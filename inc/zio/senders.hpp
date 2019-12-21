/** Senders provide functional objects to send data simply.

*/

#ifndef ZIO_SENDERS_HPP_SEEN
#define ZIO_SENDERS_HPP_SEEN

#include "zio/message.hpp"
#include "zio/port.hpp"

namespace zio {

    struct TextSender {
        Message msg;
        portptr_t port;
        TextSender() {}
        TextSender(portptr_t p, origin_t origin);
        void operator()(zio::level::MessageLevel lvl, const std::string& log);

    };
    struct JsonSender {
        Message msg;
        portptr_t port;
        JsonSender() {}
        JsonSender(portptr_t p, origin_t origin);
        void operator()(zio::level::MessageLevel lvl, const zio::json& met);
    };

}

#endif
