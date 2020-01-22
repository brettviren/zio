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


        class Flow {
        public:
            /*!
              @brief create a flow.
            */
            Flow(portptr_t port);
            ~Flow();

            /*!  
              @brief send a BOT

              Client calls send_bot() first, server calls send_bot() second;
            */
            void send_bot(Message& bot);

            /*!
              @brief receive a BOT

              A timeout, -1 waits forever.  Return false if timeout
              occurs.

              Server calls send_bot() first, client calls send_bot() second;
            */
            bool recv_bot(Message& bot, int timeout=-1);

            /*!
              @brief put a payload message into the flow

              Return false if an EOT was received in the process.
            */
            bool put(Message& dat);
            
            /*!
              @brief recv any waiting PAY messages

              A sender will slurp prior to a send of a DAT but the
              application may call this at any time after BOT.  Number
              of credits slurped is returned.  A -1 indicates EOT,
              which if app calls should respond.  A -2 indicates
              protocol error.
            */
            int slurp_pay(int timeout);

            /*!
              @brief get a payload message from the flow

              Return false immediately if an EOT was received instead.

              Negative timeout waits forever, otherwise gives timeout
              in milliseconds to wait for a FLOW message.
            */
            bool get(Message& dat, int timeout=-1);

            /*!
              @brief send any accumulated credit as a PAY

              A recver will flush pay prior to any get but the
              application may do this at any time after BOT.  Number
              of credits sent is returned.  This does not block.
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
            bool recv_eot(Message& msg, int timeout=-1);

            bool is_sender() const { return m_sender; }
            int credit() const { return m_credit; }
            int total_credit() const { return m_total_credit; }
        private:
            portptr_t m_port;
            int m_credit, m_total_credit;
            bool m_sender;      // false if we are recver

            // A Flow can use a SERVER socket.  The BOT in recv() sets
            // this.  It will be set on the message prior to any
            // subsequent send()
            Message::routing_id_t m_rid;

            bool parse_label(Message& msg, zio::json& lobj);
        };

    }
}

#endif

