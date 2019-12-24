#ifndef ZIO_EXTRACT_HPP_SEEN
#define ZIO_EXTRACT_HPP_SEEN

#include "zio/port.hpp"
#include "zio/message.hpp"
#include "zio/types.hpp"

namespace zio {

    /*!
      @brief augmented port on a CLIENT socket to extract data to a SERVER

     */

    class ExtractClient {
    public:

        /** Create a client side of the ZIO data flow extraction
            protocol.
        */
        ExtractClient(portptr_t port);
        ~ExtractClient();

        /// Send BOT and PAY initial credits
        void bot(int credits, std::vector<std::string> streams,
                 zio::json extra = {});

        // Try to send a data message.
        void send(Message& msg);
        
        // Terminate transmission.  Return false if timeout occurs. 
        bool eot(int timeout=-1);

        bool slurp_credit(int timeout=0);

    private:
        portptr_t m_port;
        int m_credits;
        const int m_total_credits;
        Message m_credmsg;
        bool m_done;
    };

    /*!
      @brief augmented port on a SERVER socket to extract data form a CLIENT
     */

    class ExtractServer {
    public:
        /*! Create a server side of the extract protocol 
         */
        ExtractServer(portptr_t port);
        ~ExtractServer();

        /// Recieve a payload data message and maybe timeout.  Return
        /// true if message recieved.  False is returned if a timeout
        /// occurs or if EOT was sent.  
        bool recv(Message& msg, int timeout=-1);

        /// Return true if end-of-transmission is reached.
        bool eot() const;
    private:
        portptr_t m_port;
        Message m_credmsg; 
        std::map<
        int m_credits, m_total_credits;
        bool m_eot;
    }

}

#endif

