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
              @brief get a payload message from the flow

              Return false immediately if an EOT was received instead.

              Negative timeout waits forever, otherwise gives timeout
              in milliseconds to wait for a FLOW message.
            */
            bool get(Message& dat, int timeout=-1);

            /*!
              @brief send EOT to other end and wait for reply

              Return false if no EOT was received, otherwise set msg
              to that EOT message.  Timeout is in milliseconds or less
              than zero to wait indefinitely.

              Note: if calling in response to an EOT sent from the
              other end, a subsequent EOT receipt is not expected.
              Use timeout=0.
            */
            bool eot(Message& msg, int timeout=-1);

            bool is_sender() const { return m_sender; }
            int credits() const { return m_credits; }
            int total_credits() const { return m_total_credits; }
        private:
            portptr_t m_port;
            int m_credits, m_total_credits;
            bool m_sender;      // false if we are recver
        };

    }
}

#endif

