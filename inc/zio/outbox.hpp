/**

   A zio outbox provides a simple, typed API to sending ZIO messages
   in ZIO supported formats (TEXT, JSON and BUFF).

 */

#ifndef ZIO_OUTBOX_HPP_SEEN
#define ZIO_OUTBOX_HPP_SEEN

#include "zio/port.hpp"

namespace zio {

    template<typename NATIVE, typename FORMAT>
    class Outbox {
        portptr_t m_port;
    public:
        Outbox(portptr_t port) : m_port(port) { }

        void operator()(level::MessageLevel lvl, const NATIVE& payload) {
            send(lvl, payload);
        }
        void send(level::MessageLevel lvl, const NATIVE& payload) {
            m_port->send(lvl, FORMAT(payload));
        }

        // bake levels semantics into method names
        void trace(const NATIVE& payload)   { send(level::trace, payload); }
        void verbose(const NATIVE& payload) { send(level::verbose, payload); }
        void debug(const NATIVE& payload)   { send(level::debug, payload); }
        void info(const NATIVE& payload)    { send(level::info, payload); }
        void summary(const NATIVE& payload) { send(level::summary, payload); }
        void warning(const NATIVE& payload) { send(level::warning, payload); }
        void error(const NATIVE& payload)   { send(level::error, payload); }
        void fatal(const NATIVE& payload)   { send(level::fatal, payload); }

    };

    // Two special types
    typedef Outbox<std::string, TEXT> Logger;
    typedef Outbox<nlohmann::json, JSON> Metric;

}

#endif
