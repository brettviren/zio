/** Test compatibility of cppzmq and CZMQ single/multipart codec. 
 */

#include "zio/cppzmq.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"
#include <czmq.h>               // normally we don't use this directly in ZIO
#include <string>
#include <vector>

int main()
{
    zio::init_all();

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
        zio::message_t cppmsg(zframe_data(frame), zframe_size(frame));
        zio::multipart_t cppmmsg = zio::multipart_t::decode(cppmsg);
        assert(cppmmsg.size() == 2);
        assert(cppmmsg.at(0).to_string() == prefix);
    }

    zframe_destroy(&frame);
    zmsg_destroy(&msg);

    zio::debug("{} {}", prefix.size(), prefix);

    for (size_t ind=0; ind<ret.size(); ++ind) {
        zio::debug("{}: {} ({})", ind, ret[ind], (int)ret[ind]);
    }

    return 0;

}
