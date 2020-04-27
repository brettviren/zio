/*!
 *  @brief ZIO data flow protocol.
 */

#ifndef ZIO_FLOW_HPP_SEEN
#define ZIO_FLOW_HPP_SEEN

#include "zio/util.hpp"
#include "zio/port.hpp"
#include <stdexcept>

namespace zio {

    namespace flow {
        /// Thrown when an EOT was received instead of expected message
        struct end_of_transmission : public virtual std::out_of_range
        {
            using std::out_of_range::out_of_range;
        };

        /// Thrown if the application violates local flow protocol.
        struct local_error : public virtual std::logic_error
        {
            using std::logic_error::logic_error;
        };
        /// Thrown if the remote side violates flow protocol
        struct remote_error : public virtual std::runtime_error
        {
            using std::runtime_error::runtime_error;
        };

        enum class direction_e : int { unknown, inject, extract };
        enum class msgtype_e : int { unknown, bot, dat, pay, eot };

        class Label
        {
            zio::Message& m_msg;
            zio::json m_fobj;
            bool m_dirty{false};

          public:
            Label(zio::Message& msg);
            ~Label();
            Label(Label&& rhs) = default;
            Label& operator=(Label&& rhs) = default;

            zio::json object() const { return m_fobj; }
            zio::json& object() { return m_fobj; }

            /// Emit a string rep
            std::string str() const;

            /// Commit any changes back to the message
            void commit();

            /// Return the direction of flow
            direction_e direction() const;

            /// Return amount of credit in message or -1 if none/error
            int credit() const;

            /// Return the flow message type
            msgtype_e msgtype() const;

            /// Set direction
            void direction(direction_e);

            /// Set the message type
            void msgtype(msgtype_e mt);

            /// Set the amount of credit
            void credit(int cred);
        };

    }  // namespace flow

    struct FlowImp;

    class Flow
    {
      public:
        /*! @brief Create one side of a data flow.
         *
         * The flow is created an initialized port, in the given
         * direction and with the given initial credit.  Depending on
         * the socket type of the port the credit will be asserted (for
         * "serverish" ports) or used as an initial recomendation
         * ("clientish" ports).
         *
         * A nominal timeout is given and used by all calls that
         * participate in the protocol.  If a differing timeout is
         * needed for a particular call, the set_timeout() method may
         * be used.
         *
         * The remaining methods interact with the flow protocol
         * (bot(), etc).  They return true on success, false if a
         * timeout occured and will throw zio::flow::protocol_error if
         * the call violates the flow protocol.
         */
        Flow(zio::portptr_t p, flow::direction_e direction, int credit,
             timeout_t tout = timeout_t{});
        ~Flow();

        Flow(Flow&& rhs);
        Flow& operator=(Flow&& rhs);

        /// Change the timeout
        void set_timeout(timeout_t tout);

        /// Return the amount of credit currently held
        int credit() const;

        /// Return the amount of total credit in use
        int total_credit() const;

        // client/server: do the bot handshake when the application
        // cares about the message content.  The payload of the passed
        // in message will be sent either as the initial client or as
        // a response by the server.  The value received is returned.
        bool bot(zio::Message& botmsg);

        // client/server: as above but application does not care about
        // the BOT message content.
        bool bot();

        /// Acknowledge an EOT received from other end.
        bool eotack();
        bool eotack(zio::Message& eotmsg);

        /// Initiate a EOT handshake (send and ack)
        bool eot();
        bool eot(zio::Message& eotmsg);

        /// Attempt to send DAT (for givers) after checking for PAY.
        bool put(zio::Message& dat);

        /// Attempt to get DAT (for takers) after flushing PAY.
        bool get(zio::Message& dat);

        /// Return the amount of credit in this flow object.
        ///
        /// This must be called if application uses low-level
        /// recv()/send() instead of put()/et().
        ///
        /// Does not block.  May throw end_of_transmission.
        ///
        /// May produce side-effect communication:
        ///
        /// If flow is "extract" (giver) then pay() attempts to
        /// receive a PAY message from socket to gain credit.  If 0 is
        /// returned by pay() then a subsequent send() of a DAT will
        /// timeout or block.  Thus, the applicatin using low-level
        /// send() must try to call pay() until it returns non-zero.
        /// If giver uses recv() then received payment is processed
        /// automatically.
        ///
        /// If flow is "inject" (taker) then pay() attempts to send a
        /// PAY message to flush any accumulated credit.  If the value
        /// total_credit() is returned then a subsequent low-level
        /// recv() will timeout or block.  If using recv() instead of
        /// get() then this MUST be called to give PAY back to other
        /// end.
        int pay();

        /// Receive a message on the flow port and push it through the
        /// flow state machine.
        ///
        /// This is is suitable for use if an application needs to
        /// poll the underlying socket.  However, it will NOT have
        /// additional side-effect communication.  In particular, it
        /// will NOT flush PAY like get().  A taker using recv() and
        /// does not explicitly and periodically call pay() will block
        /// the flow.  In particular an initial pay() is needed to get
        /// flow started.
        bool recv(zio::Message& msg);

        /// Push a message through the flow state machine and then
        /// send it to the flow port if the state machine accepts the message.
        ///
        /// Applications may use this when they must send in a way
        /// that is less coupled to the state machine.  They must then
        /// be resilient to mistakes.  Also not that no side-effect
        /// communication is performed.  In particular sending DAT
        /// does NOT also cause PAY to be received.  Such applications
        /// must explicitly and periodically call pay().
        bool send(zio::Message& msg);

      private:
        portptr_t port;

        // Use pimpl pattern to access FSM to save on recompiling
        // boost.sml code.
        std::unique_ptr<FlowImp> imp;
    };

}  // namespace zio

#endif
