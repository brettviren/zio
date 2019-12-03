#include "zio/socket.hpp"

int main()
{
    {
        zio::Socket sock(ZMQ_PUB);
        assert(sock.zsock());
        zio::address_t fqa = sock.bind("tcp://127.0.0.1:*");
        assert(fqa.size());
    }
        

    return 0;
}
