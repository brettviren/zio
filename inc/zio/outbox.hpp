/**

   The zio outboxes provide a type specific sender wrapper around a socket.

 */

#ifndef ZIO_OUTBOX_HPP_SEEN
#define ZIO_OUTBOX_HPP_SEEN

#include "zio/port.hpp"

namespace zio {

    template<typename CONVERTER>
    class Outbox {
    public:
        typedef CONVERTER converter_type;
        typedef typename converter_type::native_type native_type;

        Outbox(portptr_t port) : m_port(port) { }

        void operator()(level::MessageLevel lvl, const native_type& payload) {
            send(lvl, payload);
        }
        void send(level::MessageLevel lvl, const native_type& payload) {
            byte_array_t buf = m_convert(payload);
            m_port->send(lvl, m_convert.format(), buf);
        }

        // bake levels semantics into method names
        void trace(const native_type& payload)   { send(level::trace, payload); }
        void verbose(const native_type& payload) { send(level::verbose, payload); }
        void debug(const native_type& payload)   { send(level::debug, payload); }
        void info(const native_type& payload)    { send(level::info, payload); }
        void summary(const native_type& payload) { send(level::summary, payload); }
        void warning(const native_type& payload) { send(level::warning, payload); }
        void error(const native_type& payload)   { send(level::error, payload); }
        void fatal(const native_type& payload)   { send(level::fatal, payload); }
    private:
        portptr_t m_port;
        converter_type m_convert;
    };

    /// Two special types.
    typedef Outbox<converter::text_t> Logger;
    typedef Outbox<converter::json_t> Metric;

}

#endif
