#ifndef ZIO_UTIL_HPP_SEEN
#define ZIO_UTIL_HPP_SEEN

#include "zio/cppzmq.hpp"
#include "zio/json.hpp"

#include <optional>
#include <string>

namespace zio {

    // The great Nlohmann's JSON.
    using json = nlohmann::json;

    /// Return the ZeroMQ socket type number for the socket.
    int sock_type(const socket_t& sock);
    std::string sock_type_name(int stype);

    // timeouts, heartbeats and other things are in units of millisecond.
    using time_unit_t = std::chrono::milliseconds;
    
    // A timeout is either a time or nothing.
    using timeout_t = std::optional<time_unit_t>;

    // default values
    const int HEARTBEAT_LIVENESS = 3;
    const time_unit_t HEARTBEAT_INTERVAL{2500};
    const time_unit_t HEARTBEAT_EXPIRY{HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS};


    // SERVER and ROUTER use different "routing IDs" to identify the
    // peer a message was received from or to which a message should
    // be sent.  SERVER uses uint32_t and ROUTER uses a message part.
    // Both are meant to be opaque to the application and we wish to
    // erase the type differences in some contexts and provide a
    // common way to in-band them in a messager (besides in an
    // envelope stack such as in domo).  
    typedef std::string remote_identity_t;
    remote_identity_t to_remid(uint32_t rid);
    uint32_t to_rid(const remote_identity_t& remid);
    std::string binstr(const std::string& s);

    // Return true if socket is like a server 
    bool is_serverish(zio::socket_t& sock);
    // Return true if socket is like a client
    bool is_clientish(zio::socket_t& sock);

    // Send clientish on a DEALER or CLIENT
    send_result_t send_clientish(socket_t& socket,
                                 multipart_t& mmsg,
                                 send_flags flags = send_flags::none);

    // Send on a CLIENT
    send_result_t send_client(socket_t& client_socket,
                              multipart_t& mmsg,
                              send_flags flags = send_flags::none);

    // Send on a DEALER
    send_result_t send_dealer(socket_t& dealer_socket,
                              multipart_t& mmsg,
                              send_flags flags = send_flags::none);

    // Send serverish on a ROUTER or SERVER
    send_result_t send_serverish(socket_t& socket,
                                 multipart_t& mmsg,
                                 const remote_identity_t& remid,
                                 send_flags flags = send_flags::none);

    // Send on a SERVER
    send_result_t send_server(socket_t& server_socket,
                              multipart_t& mmsg,
                              const remote_identity_t& remid,
                              send_flags flags = send_flags::none);

    // Send on a ROUTER
    send_result_t send_router(socket_t& router_socket,
                              multipart_t& mmsg,
                              const remote_identity_t& remid,
                              send_flags flags = send_flags::none);


    // Receive clientish on a DEALER or CLIENT
    recv_result_t recv_clientish(socket_t& socket,
                                 multipart_t& mmsg,
                                 recv_flags flags = recv_flags::none);

    // Receive on a CLIENT
    recv_result_t recv_client(socket_t& client_socket,
                              multipart_t& mmsg,
                              recv_flags flags = recv_flags::none);

    // Receive on a DEALER
    recv_result_t recv_dealer(socket_t& dealer_socket,
                              multipart_t& mmsg,
                              recv_flags flags = recv_flags::none);
    
    // Receive serverish on a ROUTER or SERVER
    recv_result_t recv_serverish(socket_t& socket,
                                 multipart_t& mmsg,
                                 remote_identity_t& remid,
                                 recv_flags flags = recv_flags::none);

    // Receive on a SERVER
    recv_result_t recv_server(socket_t& server_socket,
                              multipart_t& mmsg,
                              remote_identity_t& remid,
                              recv_flags flags = recv_flags::none);

    // Receive on a ROUTER
    recv_result_t recv_router(socket_t& router_socket,
                              multipart_t& mmsg,
                              remote_identity_t& remid,
                              recv_flags flags = recv_flags::none);
    


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

    /*! Catch signals and set interrupted to true.
     *
     *  This should be called from main().  For higher level
     *  interface, see zio::init_signals() or zio::init_all() from
     *  zio/main.hpp. */
    void catch_signals ();


}

#endif
