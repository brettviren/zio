#include <czmq.h>
#include <string>
#include <iomanip>
#include <iostream>
using namespace std;
int main()
{
    zsock_t* o = zsock_new(ZMQ_PAIR);
    zsock_bind(o, "inproc://teststr");
    zsock_t* i = zsock_new(ZMQ_PAIR);
    zsock_connect(i, "inproc://teststr");

    // from port::send()
    zmsg_t* msg = zmsg_new();
    zmsg_addstrf(msg, "ZIO%d%s%s", 4, "TEXT", "Live long int and be unsigned");
    uint64_t coords[2] = {0xf12edeadbeaf, (uint64_t)zclock_time()};
    zmsg_addmem(msg, coords, 2 * sizeof(uint64_t));
    zmsg_addmem(msg, NULL, 0);
    zmsg_send(&msg, o);

    msg = zmsg_recv(i);
    char* got = zmsg_popstr(msg);
    std::string gots = got;
    free(got);
    cerr << "Size: " << gots.size() << " " << strlen(got) << ": " << gots
         << endl;

    char lvl = gots[3];
    std::string format = gots.substr(4, 4);
    std::string label = gots.substr(8);
    cerr << "h1: L" << lvl << " " << format << " \"" << label << "\"\n";

    zframe_t* ogf = zmsg_pop(msg);
    uint64_t* og = (uint64_t*)zframe_data(ogf);  // not at all safe
    uint64_t origin = og[0], granule = og[1];
    zframe_destroy(&ogf);

    cerr << "h2: " << std::hex << std::uppercase << " o=0X" << origin << " ("
         << std::dec << origin << " )" << std::hex << std::uppercase << " g=0X"
         << granule << " (" << std::dec << granule << " )" << endl;

    int nleft = zmsg_size(msg);
    cerr << "pay: " << nleft << " frames" << endl;
    while (nleft) {
        --nleft;
        zframe_t* frame = zmsg_pop(msg);
        cerr << "\tframe size: " << zframe_size(frame) << endl;
        zframe_destroy(&frame);
    }

    zmsg_destroy(&msg);
    zsock_destroy(&i);
    zsock_destroy(&o);
    return 0;
}
