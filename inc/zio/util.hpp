#ifndef ZIO_UTIL_HPP_SEEN
#define ZIO_UTIL_HPP_SEEN

#include "zio/zmq_addon.hpp"

#include <string>

namespace zio {

    // timeouts, heartbeats and other things are in units of millisecond.
    typedef std::chrono::milliseconds time_unit_t;
    
    // default values
    const int HEARTBEAT_LIVENESS = 3;
    const time_unit_t HEARTBEAT_INTERVAL{2500};
    const time_unit_t HEARTBEAT_EXPIRY{HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS};


    // Erase the difference between SERVER routing ID and ROUTER
    // envelop stack.  The former is uint32_t which we stuff in to a
    // string of length 4.  The latter is already a string.  The
    // remote identity value should be treated as opaque outside of
    // the send/recv methods.  In general, it is not printable.
    typedef std::string remote_identity_t;


    // Receive on a ROUTER or SERVER
    remote_identity_t recv_serverish(zmq::socket_t& socket,
                                     zmq::multipart_t& mmsg);

    // Receive on a SERVER
    remote_identity_t recv_server(zmq::socket_t& server_socket,
                                  zmq::multipart_t& mmsg);

    // Receive on a ROUTER
    remote_identity_t recv_router(zmq::socket_t& router_socket,
                                  zmq::multipart_t& mmsg);
    

    // Send on a ROUTER or SERVER
    void send_serverish(zmq::socket_t& socket,
                     zmq::multipart_t& mmsg,
                     remote_identity_t rid);

    // Send on a SERVER
    void send_server(zmq::socket_t& server_socket,
                     zmq::multipart_t& mmsg,
                     remote_identity_t rid);

    // Send on a ROUTER
    void send_router(zmq::socket_t& router_socket,
                     zmq::multipart_t& mmsg,
                     remote_identity_t rid);


    // Receive on a DEALER or CLIENT
    void recv_clientish(zmq::socket_t& socket,
                        zmq::multipart_t& mmsg);

    // Receive on a CLIENT
    void recv_client(zmq::socket_t& client_socket,
                     zmq::multipart_t& mmsg);

    // Receive on a DEALER
    void  recv_dealer(zmq::socket_t& dealer_socket,
                      zmq::multipart_t& mmsg);
    

    // Send on a DEALER or CLIENT
    void send_clientish(zmq::socket_t& socket,
                        zmq::multipart_t& mmsg);

    // Send on a CLIENT
    void send_client(zmq::socket_t& client_socket,
                     zmq::multipart_t& mmsg);

    // Send on a DEALER
    void send_dealer(zmq::socket_t& dealer_socket,
                     zmq::multipart_t& mmsg);


    /*! Current system time in milliseconds. */
    std::chrono::milliseconds now_ms();
    /*! Current system time in microseconds. */
    std::chrono::microseconds now_us();

    /*! Sleep a while */
    void sleep_ms(std::chrono::milliseconds zzz);

    /*! Return true when a signal has been sent to the application.
     *
     * Exit main loop if ever true.
     *
     * The catch_signals() function must be called in main() for
     * this to ever return true.
     */
    bool interrupted();

    /*! Catch signals and set interrupted to true. */
    void catch_signals ();


}

#endif
