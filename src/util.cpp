#include "zio/util.hpp"
#include "zio/logging.hpp"
#include <chrono>
#include <thread>
#include <signal.h>

using namespace zio;


remote_identity_t zio::recv_serverish(zio::socket_t& sock,
                                      zio::multipart_t& mmsg)
{
    int stype = sock.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_SERVER == stype) {
        return recv_server(sock, mmsg);
    }
    if(ZMQ_ROUTER == stype) {
        return recv_router(sock, mmsg);
    }
    throw std::runtime_error("recv requires SERVER or ROUTER socket");
}


remote_identity_t zio::recv_server(zio::socket_t& server_socket,
                                   zio::multipart_t& mmsg)
{
    zio::message_t msg;
    auto res = server_socket.recv(msg, zio::recv_flags::none);
    uint32_t routing_id = msg.routing_id();
    remote_identity_t rid;
    rid.push_back((0xff000000&routing_id) >> 24);
    rid.push_back((0x00ff0000&routing_id) >> 16);
    rid.push_back((0x0000ff00&routing_id) >> 8);
    rid.push_back((0x000000ff&routing_id));


    mmsg.decode_append(msg);
    {
        std::stringstream ss;
        ss << "zio::recv SERVER msg size " << msg.size()
           << ", " << mmsg.size() << " parts \"" << rid << "\"";
        // << " " << (void*)routing_id 
        // << " '" << (int)rid[0] << "'"
        // << " '" << (int)rid[1] << "'"
        // << " '" << (int)rid[2] << "'"
        // << " '" << (int)rid[3] << "'";
        console_log log;
        log.level = console_log::log_level::debug;
        log.debug(ss.str());
    }
    return rid;
}

remote_identity_t zio::recv_router(zio::socket_t& router_socket,
                                   zio::multipart_t& mmsg)
{
    mmsg.recv(router_socket);
    remote_identity_t rid = mmsg.popstr();
    mmsg.pop();                 // empty
    return rid;
}


void zio::send_serverish(zio::socket_t& sock,
                         zio::multipart_t& mmsg, remote_identity_t rid)
{
    int stype = sock.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_SERVER == stype) {
        return send_server(sock, mmsg, rid);
    }
    if(ZMQ_ROUTER == stype) {
        return send_router(sock, mmsg, rid);
    }
    throw std::runtime_error("send requires SERVER or ROUTER socket");
}

void zio::send_server(zio::socket_t& server_socket,
                      zio::multipart_t& mmsg, remote_identity_t rid)
{
    zio::message_t msg = mmsg.encode();
    uint32_t routing_id =
        0xff000000&(rid[0] << 24) |
        0x00ff0000&(rid[1] << 16) |
        0x0000ff00&(rid[2] << 8) |
        0x000000ff&rid[3];
    msg.set_routing_id(routing_id);
    {
        std::stringstream ss;
        ss << "zio::send SERVER msg size " << msg.size()
           << ", " << mmsg.size() << " parts \"" << rid << "\"";
        // << " " << (void*)routing_id 
        // << " '" << (int)rid[0] << "'"
        // << " '" << (int)rid[1] << "'"
        // << " '" << (int)rid[2] << "'"
        // << " '" << (int)rid[3] << "'";
        console_log log;
        log.debug(ss.str());
    }
    server_socket.send(msg, zio::send_flags::none);
}

void zio::send_router(zio::socket_t& router_socket,
                      zio::multipart_t& mmsg, remote_identity_t rid)
{
    mmsg.pushmem(NULL, 0);
    mmsg.pushstr(rid);
    mmsg.send(router_socket);
}


void zio::recv_clientish(zio::socket_t& socket,
                         zio::multipart_t& mmsg)
{
    int stype = socket.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_CLIENT == stype) {
        recv_client(socket, mmsg);
        return;
    }
    if(ZMQ_DEALER == stype) {
        recv_dealer(socket, mmsg);
        return;
    }
    throw std::runtime_error("recv requires CLIENT or DEALER socket");
}

void zio::recv_client(zio::socket_t& client_socket,
                      zio::multipart_t& mmsg)
{
    zio::message_t msg;
    auto res = client_socket.recv(msg, zio::recv_flags::none);
    mmsg.decode_append(msg);
    {
        std::stringstream ss;
        ss << "zio::recv CLIENT msg size " << msg.size()
           << ", " << mmsg.size() << " parts";
        console_log log;
        log.debug(ss.str());
    }
    return;
}


void zio::recv_dealer(zio::socket_t& dealer_socket,
                      zio::multipart_t& mmsg)
{
    mmsg.recv(dealer_socket);
    mmsg.pop();                 // fake being REQ
    return;
}
    

void zio::send_clientish(zio::socket_t& socket,
                         zio::multipart_t& mmsg)
{
    int stype = socket.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_CLIENT == stype) {
        send_client(socket, mmsg);
        return;
    }
    if(ZMQ_DEALER == stype) {
        send_dealer(socket, mmsg);
        return;
    }
    throw std::runtime_error("send requires CLIENT or DEALER socket");
}

void zio::send_client(zio::socket_t& client_socket,
                      zio::multipart_t& mmsg)
{
    zio::message_t msg = mmsg.encode();
    {
        std::stringstream ss;
        ss << "zio::send CLIENT msg size " << msg.size()
           << ", " << mmsg.size() << " parts";
        console_log log;
        log.debug(ss.str());
    }
    client_socket.send(msg, zio::send_flags::none);
}

void zio::send_dealer(zio::socket_t& dealer_socket,
                      zio::multipart_t& mmsg)
{
    mmsg.pushmem(NULL,0);       // pretend to be REQ
    mmsg.send(dealer_socket);
}


std::chrono::milliseconds zio::now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}
std::chrono::microseconds zio::now_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
}

void zio::sleep_ms(std::chrono::milliseconds zzz)
{
    std::this_thread::sleep_for(zzz);
}

static int s_interrupted = 0;
static
void s_signal_handler (int signal_value)
{
    s_interrupted = 1;
}
bool zio::interrupted() 
{
    return s_interrupted==1; 
}

// Call from main()
void zio::catch_signals ()
{
    struct sigaction action;
    action.sa_handler = s_signal_handler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGTERM, &action, NULL);
}
