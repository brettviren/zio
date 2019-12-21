#include "zio/message.hpp"
#include <czmq.h>
#include <cassert>

int main()
{
    zsys_init();

    std::string label = "Extra spicy";

    zio::Message msg({{zio::level::MessageLevel(4),"TEXT",label},
                      {0xdeadbeaf,1234,0}});
    std::string spl = "Recipe for grandma's hot wings";
    msg.payload().emplace_back(spl.begin(), spl.end());


    auto hh = msg.header();
    assert (hh.prefix.level == 4);
    assert (hh.prefix.format == "TEXT");
    assert (hh.coord.seqno == 0);
    assert(!msg.payload().empty());
    {
        std::string s(msg.payload()[0].begin(),msg.payload()[0].end());
        assert(s == spl);
    }

    auto raw = msg.encode();
    for (size_t ind=0; ind<raw.size(); ++ind) {
        zsys_debug("%d: [%c] (%d)", ind, (char)raw[ind], (int)raw[ind]);
    }

    msg.decode(raw);

    assert (hh.prefix.level == 4);
    assert (hh.prefix.format == "TEXT");
    assert (hh.coord.seqno == 0);
    assert(!msg.payload().empty());
    {
        std::string s(msg.payload()[0].begin(),msg.payload()[0].end());
        assert(s == spl);
    }

    return 0;
}
