/*!
  @brief ZIO data flow protocol.
*/

#ifndef ZIO_FLOW_HPP_SEEN
#define ZIO_FLOW_HPP_SEEN

#include "zio/util.hpp"
#include "zio/port.hpp"
#include <stdexcept>

namespace zio {

    namespace flow {
        /// Thrown when an EOT was received instead of expected message
        struct end_of_transmission : public virtual std::out_of_range {
            using std::out_of_range::out_of_range ;
        };

        /// Thrown if the application violates local flow protocol.
        struct local_error : public virtual std::logic_error {
            using std::logic_error::logic_error ;
        };
        /// Thrown if the remote side violates flow protocol
        struct remote_error : public virtual std::runtime_error {
            using std::runtime_error::runtime_error ;
        };

        enum class direction_e { inject, extract };

    }

    struct FlowImp;

    class Flow {
    public:
        /*! @brief Create one side of a data flow.

          The flow is created an initialized port, in the given
          direction and with the given initial credit.  Depending on
          the socket type of the port the credit will be asserted (for
          "serverish" ports) or used as an initial recomendation
          ("clientish" ports).

          A nominal timeout is given and used by all calls that
          participate in the protocol.  If a differing timeout is
          needed for a particular call, the set_timeout() method may
          be used.

          The remaining methods interact with the flow protocol
          (bot(), etc).  They return true on success, false if a
          timeout occured and will throw zio::flow::protocol_error if
          the call violates the flow protocol.
          
        */
        Flow(zio::portptr_t p, flow::direction_e direction, size_t credit,
             timeout_t tout = timeout_t{});
        
        /// Change the timeout
        void set_timeout(timeout_t tout);

        /// Return the amount of credit currently held
        size_t credit() const;

        /// Return the amount of total credit in use
        size_t total_credit() const;

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

        /// Attempt to send DAT (for givers)
        bool put(zio::Message& dat);

        /// Attempt to get DAT (for takers)
        bool get(zio::Message& dat);


        /// Receive a message on the flow port and push it through the
        /// flow state machine.  
        bool recv(zio::Message& msg);

        /// Push a message through the flow state machine and then
        /// send it to the flow port.  Applications should take care
        /// to use this while respecting the state machine.  Best to
        /// use the three letter methods.
        bool send(zio::Message& msg);

    private:
        portptr_t port;

        // Use pimpl pattern to access FSM to save on recompiling
        // boost.sml code.
        std::unique_ptr<FlowImp> imp;        


    };
    

}

#endif

