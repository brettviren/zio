// Test the speed of msg churn needed to encode/decode multi frame
// messages into single frame messages.

#include <czmq.h>
#include <vector>

int main()
{
    zsys_init();
    const int HEADERSIZE=32;
    char* header[HEADERSIZE] = {0};
    const int NTRY = 10000;
    const int MAXSIZE = 1<<17;
    int size = 2;
    auto t0 = zclock_usecs();
    while (size <= MAXSIZE) {
        auto t1 = zclock_usecs();        
        std::vector<char> buf(size);
        for (int count=0; count<NTRY; ++count) {
            zmsg_t* msg = zmsg_new();
            zframe_t* frame = zframe_new(header, HEADERSIZE);
            zmsg_append(msg, &frame);
            frame = zframe_new(buf.data(), size);
            zmsg_append(msg, &frame);
            frame = zmsg_encode(msg);
            zmsg_t* msg2 = zmsg_new();
            zmsg_append(msg2, &frame);
            zmsg_destroy(&msg);
            msg = zmsg_decode(zmsg_first(msg2));
            zmsg_destroy(&msg2);
            zmsg_destroy(&msg);
        }
        auto t2 = zclock_usecs();
        zsys_info("size: %10d in %ld usec %.3f kHz",
                  size, (t2-t1), (1000.0*NTRY)/(t2-t1));
        size *= 2;
    }
    auto t3 = zclock_usecs();
    zsys_info("%.3f sec", 1e-6*(t3-t0));
    return 0;
}
