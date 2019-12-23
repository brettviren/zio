#ifndef ZIO_OUTBOX_HPP_SEEN
#define ZIO_OUTBOX_HPP_SEEN

#include "zio/message.hpp"
#include "zio/types.hpp"
#include <functional>

namespace zio {

    /*!
      @brief output objects of a fixed native type with levels expressed as methods.

      An Outbox provides a "logger" like object to simplfy use in code
      to send out messages of a fixed type.

      It is templated on a native type and given a functional object
      to handle actual sending.  This object should convert from
      native type to Message.

    */
    template<typename NATIVE>
    class Outbox {
    public:
        typedef NATIVE native_type;
        typedef typename std::function<void(zio::level::MessageLevel lvl,
                                            const native_type&)> sender_type;

        explicit Outbox(const sender_type& sender) : m_send(sender) { }
        virtual ~Outbox() { }

        void operator()(level::MessageLevel lvl, const native_type& nat) {
            send(lvl, nat);
        }
        virtual void send(level::MessageLevel lvl, const native_type& nat) {
            m_send(lvl, nat);
        }

        // bake levels semantics into method names
        void trace(const native_type& nat)
            { send(level::trace, nat); }
        void verbose(const native_type& nat)
            { send(level::verbose, nat); }
        void debug(const native_type& nat)
            { send(level::debug, nat); }
        void info(const native_type& nat)
            { send(level::info, nat); }
        void summary(const native_type& nat)
            { send(level::summary, nat); }
        void warning(const native_type& nat)
            { send(level::warning, nat); }
        void error(const native_type& nat)
            { send(level::error, nat); }
        void fatal(const native_type& nat)
            { send(level::fatal, nat); }

    private:
        sender_type m_send;
    };

    /*!
      @brief two special types of outboxes.
    */

    /// A text based logger to use like print().
    typedef Outbox<std::string> Logger;

    /// Emit structured data.
    typedef Outbox<zio::json> Metric;

}

#endif
