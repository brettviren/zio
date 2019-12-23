#ifndef ZIO_EXTRACT_HPP_SEEN
#define ZIO_EXTRACT_HPP_SEEN

#include "zio/port.hpp"
#include "zio/message.hpp"
#include "zio/types.hpp"

namespace zio {

    /*!
      @brief augmented port on a CLIENT socket to extract data to a SERVER

     */

    class Extract {
    public:

        // Create a client side of the extract protocol with given
        // number of credits and on an already attached CLIENT port.
        // Extra attributes may be specified for the initial REQ
        // message.
        Extract(portptr_t port, int credits, zio::json extra = {});
        ~Extract();

        // Try to send a data message.
        bool send(Message& msg);
        
        // Terminate transmission.  Return false if timeout occurs. 
        bool eot(int timeout=-1);

        void slurp_credit(int timeout=0);

    private:
        portptr_t m_port;
        int m_credits;
        const int m_total_credits;
        Message m_credmsg;
        bool m_done;
    };

}

#endif

