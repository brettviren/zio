#include "zio/message.hpp"
#include <czmq.h>
#include <cassert>

#include <iostream>

using namespace std;

int main()
{


    std::string label = "Extra spicy";

    zio::Message msg({{zio::level::MessageLevel(4),"TEXT",label},
                      {0xdeadbeaf,1234,0}});
    std::string spl = "Recipe for grandma's hot wings";
    msg.add(zio::message_t(spl.data(), spl.size()));

    assert (msg.level() == 4);
    assert (msg.format() == "TEXT");
    assert (msg.seqno() == 0);
    assert(!msg.payload().empty());
    {
        std::string s(msg.payload()[0].data<const char>(),msg.payload()[0].size());
        assert(s == spl);
    }

    auto spmsg = msg.encode();
    char* raw = spmsg.data<char>();
    size_t raw_size = spmsg.size();
    for (size_t ind=0; ind<raw_size; ++ind) {
        char c = raw[ind];
        cerr << ind << "[" << c << "] (" << (int)c << ")\n";
    }

    msg.decode(spmsg);


    assert (msg.level() == 4);
    assert (msg.format() == "TEXT");
    assert (msg.seqno() == 0);
    assert(!msg.payload().empty());
    {
        std::string s(msg.payload()[0].data<const char>(),msg.payload()[0].size());
        assert(s == spl);
    }

    zio::json lobj{{"flow","EOT"},{"direction","extract"}};
    msg.set_label(lobj.dump());
    auto obj = zio::json::parse(msg.label());
    assert(obj["flow"] == "EOT");
    assert(obj["direction"] == "extract");
    obj["direction"] = "inject";
    cerr <<msg.label()<< endl;
    msg.set_label(obj.dump());
    obj = zio::json::parse(msg.label());
    assert(obj["direction"] == "inject");
    cerr <<msg.label()<< endl;
    return 0;
}
