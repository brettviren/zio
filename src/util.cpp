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
        "PAIR",   "PUB",  "SUB",    "REQ",     "REP",
        "DEALER",  // 5
        "ROUTER",  // 6
        "PULL",   "PUSH", "XPUB",   "XSUB",    "STREAM",
        "SERVER",  // 12
        "CLIENT",  // 13
        "RADIO",  "DISH", "GATHER", "SCATTER", "DGRAM",  0};
    if (stype < 0 or stype > 18) return "";
    return names[stype];
}

zio::remote_identity_t zio::to_remid(uint32_t rid)
{
    return std::string(reinterpret_cast<const char*>(&rid), sizeof(uint32_t));
}
uint32_t zio::to_rid(const zio::remote_identity_t& remid)
{
    if (remid.size() != sizeof(uint32_t)) { return 0; }
    return *reinterpret_cast<const uint32_t*>(remid.data());
}
std::string zio::binstr(const std::string& s)
{
    std::stringstream ss;
    for (size_t ind = 0; ind < s.size(); ++ind) {
        ss << std::to_string((uint8_t)s[ind]) << " ";
    }
    return ss.str();
}

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

zio::recv_result_t zio::recv_serverish(zio::socket_t& sock,
                                       zio::multipart_t& mmsg,
                                       zio::remote_identity_t& remid,
                                       recv_flags flags)
{
    int stype = sock.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_SERVER == stype) { return recv_server(sock, mmsg, remid); }
    if (ZMQ_ROUTER == stype) { return recv_router(sock, mmsg, remid); }
    throw std::runtime_error("recv requires SERVER or ROUTER socket");
}

zio::recv_result_t zio::recv_server(zio::socket_t& server_socket,
                                    zio::multipart_t& mmsg,
                                    zio::remote_identity_t& remid,
                                    recv_flags flags)
{
    zio::message_t msg;
    auto res = server_socket.recv(msg, zio::recv_flags::none);
    if (!res) { return res; }
    remid = to_remid(msg.routing_id());
    mmsg.decode_append(msg);
    return res;
}

zio::recv_result_t zio::recv_router(zio::socket_t& router_socket,
                                    zio::multipart_t& mmsg,
                                    zio::remote_identity_t& remid,
                                    recv_flags flags)
{
    auto res = mmsg.recv(router_socket);
    if (!res) { return res; }
    remid = mmsg.popstr();
    mmsg.pop();  // delimiter
    return res;
}

zio::send_result_t zio::send_serverish(zio::socket_t& sock,
                                       zio::multipart_t& mmsg,
                                       const zio::remote_identity_t& remid,
                                       send_flags flags)
{
    int stype = sock.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_SERVER == stype) { return send_server(sock, mmsg, remid); }
    if (ZMQ_ROUTER == stype) { return send_router(sock, mmsg, remid); }
    throw std::runtime_error("send requires SERVER or ROUTER socket");
}

zio::send_result_t zio::send_server(zio::socket_t& server_socket,
                                    zio::multipart_t& mmsg,
                                    const zio::remote_identity_t& remid,
                                    send_flags flags)
{
    if (remid.empty()) {
        throw std::runtime_error("send server requires a remote identity");
    }
    zio::message_t msg = mmsg.encode();
    msg.set_routing_id(to_rid(remid));
    return server_socket.send(msg, zio::send_flags::none);
}

zio::send_result_t zio::send_router(zio::socket_t& router_socket,
                                    zio::multipart_t& mmsg,
                                    const zio::remote_identity_t& remid,
                                    send_flags flags)
{
    mmsg.pushmem(NULL, 0);  // delimiter
    mmsg.pushstr(remid);
    return mmsg.send(router_socket);
}

zio::recv_result_t zio::recv_clientish(zio::socket_t& socket,
                                       zio::multipart_t& mmsg, recv_flags flags)
{
    int stype = socket.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_CLIENT == stype) { return recv_client(socket, mmsg); }
    if (ZMQ_DEALER == stype) { return recv_dealer(socket, mmsg); }
    throw std::runtime_error("recv requires CLIENT or DEALER socket");
}

zio::recv_result_t zio::recv_client(zio::socket_t& client_socket,
                                    zio::multipart_t& mmsg, recv_flags flags)
{
    zio::message_t msg;
    auto res = client_socket.recv(msg, zio::recv_flags::none);
    if (!res) { return res; }
    mmsg.decode_append(msg);
    return res;
}

zio::recv_result_t zio::recv_dealer(zio::socket_t& dealer_socket,
                                    zio::multipart_t& mmsg, recv_flags flags)
{
    auto res = mmsg.recv(dealer_socket);
    if (!res) { return res; }
    mmsg.pop();  // fake being REQ
    return res;
}

zio::send_result_t zio::send_clientish(zio::socket_t& socket,
                                       zio::multipart_t& mmsg, send_flags flags)
{
    int stype = socket.getsockopt<int>(ZMQ_TYPE);
    if (ZMQ_CLIENT == stype) { return send_client(socket, mmsg); }
    if (ZMQ_DEALER == stype) { return send_dealer(socket, mmsg); }
    throw std::runtime_error("send requires CLIENT or DEALER socket");
}

zio::send_result_t zio::send_client(zio::socket_t& client_socket,
                                    zio::multipart_t& mmsg, send_flags flags)
{
    zio::message_t msg = mmsg.encode();
    return client_socket.send(msg, zio::send_flags::none);
}

zio::send_result_t zio::send_dealer(zio::socket_t& dealer_socket,
                                    zio::multipart_t& mmsg, send_flags flags)
{
    mmsg.pushmem(NULL, 0);  // pretend to be REQ
    return mmsg.send(dealer_socket);
}

std::chrono::milliseconds zio::now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}
std::chrono::microseconds zio::now_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

void zio::sleep_ms(std::chrono::milliseconds zzz)
{
    std::this_thread::sleep_for(zzz);
}

static int s_interrupted = 0;
static void s_signal_handler(int signal_value) { s_interrupted = 1; }
bool zio::interrupted() { return s_interrupted == 1; }

// Call from main()
void zio::catch_signals()
{
    struct sigaction action;
    action.sa_handler = s_signal_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}
