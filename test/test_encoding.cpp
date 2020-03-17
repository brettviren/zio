#include <czmq.h>
#include "zio/zmq_addon.hpp"
#include <string>
#include <vector>
#include <iostream>
int main()
{
    const std::string prefix="ZIO4TEXTTo be or not to be that ?"; // length 33, ascii 33 is !

    zmsg_t* msg = zmsg_new();
    zmsg_addstr(msg, prefix.c_str());
    struct XYZ {
        uint64_t a{0x6644444444444444},b{0x5555555555555555}, c{0x4466666666666666};
    } xyz;
    zmsg_addmem(msg, &xyz, sizeof(XYZ));
    zframe_t* frame = zmsg_encode(msg);
    std::vector<std::uint8_t> ret(zframe_data(frame), zframe_data(frame) + zframe_size(frame));

    {
        zmq::message_t cppmsg(zframe_data(frame), zframe_size(frame));
        zmq::multipart_t cppmmsg = zmq::multipart_t::decode(cppmsg);
        assert(cppmmsg.size() == 2);
        assert(cppmmsg.at(0).to_string() == prefix);
    }

    zframe_destroy(&frame);
    zmsg_destroy(&msg);

    std::cout << prefix.size() << " " << prefix << std::endl;

    for (size_t ind=0; ind<ret.size(); ++ind) {
        std::cout << ind << ": " << ret[ind] << " (" << (int)ret[ind] << ")" << std::endl;
    }

    return 0;

}
