// Test the speed of msg churn needed to encode/decode multi frame
// messages into single frame messages.

#include "zio/cppzmq.hpp"
#include "zio/util.hpp"
#include <vector>

#include <iostream>

int main()
{
    const int HEADERSIZE=32;
    char* header[HEADERSIZE] = {0};
    const int NTRY = 10000;
    const int MAXSIZE = 1<<17;
    int size = 2;
    const auto t0 = zio::now_us();
    while (size <= MAXSIZE) {
        const auto t1 = zio::now_us();
        std::vector<char> buf(size);
        for (int count=0; count<NTRY; ++count) {
            zio::multipart_t mmsg;
            mmsg.addmem(header, HEADERSIZE);
            mmsg.addmem(buf.data(), size);
            auto msg = mmsg.encode();

            auto mmsg2 = zio::multipart_t::decode(msg);
        }
        const auto t2 = zio::now_us();
        const auto dt = (t2-t1).count();
        std::cerr << "size: " << size
                  << " in " << dt
                  << " usec " << (1000.0*NTRY)/dt << " kHz\n";
        size *= 2;
    }
    const auto t3 = zio::now_us();
    const auto tott = 1e-6*(t3-t0).count();
    std::cerr << tott << " sec\n";

    return 0;
}
