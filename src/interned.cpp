#include "zio/interned.hpp"




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
        0
    };
    if (stype < 0 or stype > 11) return "";
    return names[stype];
}
