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

        /*! 

          @brief send data flow protocol messages.
      
          A sender does not implement protocol nor manage credit, it
          merely provides methods to send protocol messages.  It may be
          created on a CLIENT or SERVER port and used for either extract
          or inject.  If a port is a SERVER and a nonzero routing ID is
          given then the ID will be set on the socket prior to sending
          mesages.

          Each message may have application dependent additional
          attributes and payload.

        */
        class Sender {
        public:
            Sender(portptr_t port, int routing_id = 0);

            /// Send BOT.  
            void bot(flow::Direction fd, int credits,
                     const zio::json& extra = zio::json{},
                     const payload_t& payload = payload_t());
        
            /// Send EOT.
            void eot(const zio::json& extra = zio::json{},
                     const payload_t& payload = payload_t());

            /// Send a PAY message.
            void pay(int credits=0,
                     const zio::json& extra = zio::json{},
                     const payload_t& payload = payload_t());

            /// Send a DAT message.
            void dat(const zio::json& extra = zio::json{},
                     const payload_t& payload = payload_t());
        
        private:
            portptr_t m_port;
            int m_rid;
            Message m_flowmsg;

            void send(const zio::json& flowobj,
                      const payload_t& payload);
        };


        // An endpoint holds state for one end of ZIO data flow protocol.
        struct endpoint_t {
            int id;             // nonzero if this is a remote client
            int credits;        // current number of credits
            int total_credits;  // max number of credits
            Direction direction; // the data flow direction (extract/inject)
            Sender send;         // use to send message other end
            Message flowmsg{};     // last received message.
        };

        /*!  

          @brief A ZIO data flow server provides a source of messages
          from multiple remote clients.

        */
        class Server {
        public:

            /// Create a data flow server on a port.
            Server(portptr_t port, int max_credits=10);

            /// Attempt to receive a message from the port.  Return
            /// nullptr if timeout occurs.  Otherwise, return pointer
            /// to structure embodying the received information.
            /// Application should use it but Server owns it and may
            /// invalidate it.
            endpoint_t* recv(int timeout=-1);

            /// Access recent info about a client by its ID number or
            /// NULL if it is not known.  Object is owned by Server
            /// and pointer will be invalidated when client is
            /// destroyed.
            endpoint_t* endpoint(int id);

            /// Destroy an endpoint, invalidating any outstanding
            /// pointers.  Note, application assumes duty to call EOT.
            void destroy(endpoint_t** ep);

        private:
            portptr_t m_port;
            int m_max_credits;

            // filled on recv of BOT, 
            std::map<int, endpoint_t> m_endpoints;
        };

        /*!  

          @brief A ZIO data flow client provides a source of messages
          from a single remote server.

        */
        class Client {
        public:

            /// Create a data flow client on a port.
            Client(portptr_t port);

            /// Attempt to recive a message from the port.  Return
            /// nullptr if timeout occurs.  Otherwise, return pointer
            /// to structure embodying the received information.
            /// Application should use it but Client owns it and may
            /// invalidate it.
            endpoint_t* recv(int timeout=-1);
        private:
            portptr_t m_port;
            endpoint_t m_endpoint;
        };        
    }
}

#endif

