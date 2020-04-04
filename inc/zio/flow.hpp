/*!
  @brief implementation of ZIO data flow protocol endpoints
 */

#ifndef ZIO_FLOW_HPP_SEEN
#define ZIO_FLOW_HPP_SEEN

#include "zio/port.hpp"
#include "zio/message.hpp"

namespace zio {

    
    /*!
      @brief ZIO data flow
    */
    namespace flow {
        enum Direction { undefined, extract, inject };

        enum MessageType { timeout, BOT, DAT, PAY, EOT, notype };
        const char* name(MessageType mt);

        [[nodiscard]]
        bool parse_label(Message& msg, zio::json& lobj);

        /*! brief return message type
         */
        MessageType type(Message& msg);

        class Flow {

        public:
            /*!
              @brief create a flow.
            */
            Flow(portptr_t port);
            ~Flow();

            /*! Receive at most one message on the flow port.

              The message is filled and the corresponding message type
              returned.  This type may be "timeout" if no message was
              available or "unexpected" if a message was received but
              it was invalid.

              The message will be processed internally.  Eg, if PAY,
              the credit is increased.
              
            */
            [[nodiscard]]
            MessageType recv(Message& msg, int timeout=-1);

            /*!  
              @brief send a BOT

              Client calls send_bot() first, server calls send_bot() second;
            */
            void send_bot(Message& bot);

            /*!
              @brief Try to receive next message as a BOT message

              Return value is BOT on success, but may be EOT or
              timeout.
            */
            [[nodiscard]]
            MessageType recv_bot(Message& msg, int timeout=-1);

            /*!
              @brief put a payload message into the flow

              Prior to sending DAT, this tries to receive PAY.  If
              credit drops to zero, flow requires no sending of DAT
              and will wait as long as the given timeout.

              Return DAT message type if message successful sent.  EOT
              is returned if EOT was received during pay slurping.  If
              timeout occurs due to no credit and timeout while
              waiting for pay, timeout is returned.
            */
            [[nodiscard]]
            MessageType put(Message& dat, int timeout = -1);
            
            /*!
              @brief Try to receive a PAY message and apply any credit

              This method can be but need not be called by the
              application as it is called prior to any attempt to get
              DAT messages.

              Message type will indicate actual message which may be.
              If application calls, it should respond to EOT.
            */
            [[nodiscard]]
            MessageType recv_pay(Message& msg, int timeout);

            /*!
              @brief Try to get a payload message from the flow

              Return false immediately if an EOT was received instead.

              Negative timeout waits forever, otherwise gives timeout
              in milliseconds to wait for a FLOW message.
            */
            [[nodiscard]]
            MessageType recv_dat(Message& dat, int timeout=-1);

            /*!
              @brief send any accumulated credit as a PAY

              Number of credits sent is returned.  

              This does not block.

              This will be called implicity in recv_dat().  An
              application may explicitly call this anytime after BOT
              handshake if it is desired for the other end to not
              block in send_dat() prior to this end calling
              recv_dat().  
            */
            int flush_pay();

            /*!
              @brief send EOT.

              If a get() or a put() was interupted by an EOT the app
              should call send_eot() as an acknowledgement and should
              not call recv_eot().  If app explicitly initates EOT
              with send_eot() then the app should call recv_eot() to
              wait for an ack from the other end.
            */
            void send_eot(Message& msg);

            
            /*!
              @brief Receive an EOT

              Return false if no EOT was received, otherwise set msg
              to that EOT message.  Timeout is in milliseconds or less
              than zero to wait indefinitely.

              Note: an app only needs to call recv_eot() if they
              explicitly initiated with send_eot(). 

            */
            [[nodiscard]]
            MessageType recv_eot(Message& msg, int timeout=-1);

            /*!
              @brief Recv until get EOT.

              Returns false if timeout.
            */
            MessageType finish(Message& msg, int timeout=-1);
            /*!
              @brief Do shutdown handshake.

              This will send and recv an EOT, waiting as timout.  
              Returns false if timeout.
            */
            MessageType close(Message& msg, int timeout=-1);

            bool is_sender() const { return m_sender; }
            int credit() const { return m_credit; }
            int total_credit() const { return m_total_credit; }
        private:
            portptr_t m_port;
            int m_credit, m_total_credit;
            bool m_sender;      // false if we are recver

            // A Flow can use a SERVER or ROUTER socket.  The remote
            // ID carried by the BOT in recv() sets it on the flow
            // object.  It will be set on the message prior to any
            // subsequent send()
            remote_identity_t m_remid;

            int m_send_seqno{-1};
            int m_recv_seqno{-1};

            // Helper to do a single receive and validate if a message
            // is available.  
            MessageType recv_one(Message& msg, int timeout);

            // These internally process a received and validated
            // message message of the given type.  These do not
            // increment m_recv_seqno.
            MessageType proc_bot(Message& bot, const zio::json& fobj);
            MessageType proc_dat(Message& dat, const zio::json& fobj);
            MessageType proc_pay(Message& pay, const zio::json& fobj);
            MessageType proc_eot(Message& eot, const zio::json& fobj);

            // enum States { CTOR, WANTBOT, OWEBOT, INI, EXTRACTING, INJECTING,
            //               FINACK, ACKFIN, FIN };
            // States m_state;
        };

    }
}

#endif

