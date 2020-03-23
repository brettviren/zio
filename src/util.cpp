#include "zio/util.hpp"
#include "zio/logging.hpp"
#include <chrono>
#include <thread>
#include <sstream>
#include <signal.h>

using namespace zio;




int zio::sock_type(const zio::socket_t& sock)
{
    return sock.getsockopt<int>(ZMQ_TYPE);
}

std::string zio::sock_type_name(int stype)
{
    static const char* names[] = {
        "PAIR",
        "PUB",
        "SUB",
        "REQ",
        "REP",
        "DEALER",
        "ROUTER",
        "PULL",
        "PUSH",
        "XPUB",
        "XSUB",
        "STREAM",
        "SERVER",
        "CLIENT",
        "RADIO",
        "DISH",
        "GATHER",
        "SCATTER",
        "DGRAM",
        0
    };
    if (stype < 0 or stype > 11) return "";
    return names[stype];
}


zio::remote_identity_t zio::to_remid(uint32_t rid)
{
    return std::string(reinterpret_cast<const char*>(&rid), sizeof(uint32_t));
}
uint32_t zio::to_rid(const zio::remote_identity_t& remid)
{
    if (remid.size() != sizeof(uint32_t)) {
        return 0;
    }
    return *reinterpret_cast<const uint32_t*>(remid.data());
}
std::string zio::binstr(const std::string& s)
{
    std::stringstream ss;
    for (size_t ind=0; ind<s.size(); ++ind) {
        ss << std::to_string((uint8_t)s[ind]) << " ";
    }
    return ss.str();
}

// zio::routing_id_t zio::to_routing(const remote_identity_t& remid)
// {
//     if (remid.empty()) {
//         return 0;
//     }
//     routing_id_t rid =
//         0xff000000&((uint8_t)remid[0] << 24) |
//         0x00ff0000&((uint8_t)remid[1] << 16) |
//         0x0000ff00&((uint8_t)remid[2] << 8) |
//         0x000000ff&((uint8_t)remid[3]);
//     return rid;
// }
// zio::remote_identity_t zio::from_routing(routing_id_t rid)
// {
//     if (rid == 0) {
//         return "";
//     }
//     std::string remid(5);
//     remid[0] = ((0xff000000&rid) >> 24);
//     remid[1] = ((0x00ff0000&rid) >> 16);
//     remid[2] = ((0x0000ff00&rid) >> 8);
//     remid[3] = ((0x000000ff&rid));
//     return remid;
// }

bool zio::is_serverish(zio::socket_t& sock)
{
    int stype = sock.getsockopt<int>(ZMQ_TYPE);
    return ZMQ_SERVER == stype or ZMQ_ROUTER == stype;
}
bool zio::is_clientish(zio::socket_t& sock)
{
    int stype = sock.getsockopt<int>(ZMQ_TYPE);
    return ZMQ_CLIENT == stype or ZMQ_DEALER == stype;
}

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
    remote_identity_t remid = to_remid(msg.routing_id());
    zio::debug("recv_server: rid={} remid='{}'", msg.routing_id(), zio::binstr(remid));
    mmsg.decode_append(msg);
    return remid;
}

remote_identity_t zio::recv_router(zio::socket_t& router_socket,
                                   zio::multipart_t& mmsg)
{
    mmsg.recv(router_socket);
    zio::debug("recv router: {} parts", mmsg.size());
    remote_identity_t remid = mmsg.popstr();
    mmsg.pop();                 // delimiter
    return remid;
}


void zio::send_serverish(zio::socket_t& sock,
                         zio::multipart_t& mmsg,
                         const zio::remote_identity_t& remid)
{
    int stype = sock.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_SERVER == stype) {
        send_server(sock, mmsg, remid);
        return;
    }
    if(ZMQ_ROUTER == stype) {
        send_router(sock, mmsg, remid);
        return;
    }
    throw std::runtime_error("send requires SERVER or ROUTER socket");
}

void zio::send_server(zio::socket_t& server_socket,
                      zio::multipart_t& mmsg,
                      const zio::remote_identity_t& remid)
{
    if (remid.empty()) {
        throw std::runtime_error("send server requires a remote identity");
    }
    zio::message_t msg = mmsg.encode();
    msg.set_routing_id(to_rid(remid));
    server_socket.send(msg, zio::send_flags::none);
}

void zio::send_router(zio::socket_t& router_socket,
                      zio::multipart_t& mmsg,
                      const zio::remote_identity_t& remid)
{
    mmsg.pushmem(NULL, 0);      // delimiter
    mmsg.pushstr(remid);
    zio::debug("send router: {} parts", mmsg.size());
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
    return;
}


void zio::recv_dealer(zio::socket_t& dealer_socket,
                      zio::multipart_t& mmsg)
{
    mmsg.recv(dealer_socket);
    zio::debug("recv dealer: {} parts", mmsg.size());
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
    client_socket.send(msg, zio::send_flags::none);
}

void zio::send_dealer(zio::socket_t& dealer_socket,
                      zio::multipart_t& mmsg)
{
    mmsg.pushmem(NULL,0);       // pretend to be REQ
    zio::debug("send dealer: {} parts", mmsg.size());
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
