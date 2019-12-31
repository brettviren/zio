#include "zio/interned.hpp"

int main()
{
    {
        zio::context_t ctx;
        zio::socket_t sock(ctx, ZMQ_PUB);
        sock.bind("tcp://127.0.0.1:*");
    }
        

    return 0;
}
