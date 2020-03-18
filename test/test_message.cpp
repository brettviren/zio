#include "zio/message.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"

#include <cassert>

int main()
{
    zio::init_all();

    std::string label = "Extra spicy";

    zio::Message msg({{zio::level::MessageLevel(4),"TEXT",label},
                      {0xdeadbeaf,1234,0}});
    std::string spl = "Recipe for grandma's hot wings";
    msg.add(zio::message_t(spl.data(), spl.size()));

    assert (msg.level() == 4);
    assert (msg.form() == "TEXT");
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
        zio::debug("{}[{}] ({})", ind, c, (int)c);
    }

    zio::Message msg2;
    msg2.decode(spmsg);

    assert (msg2.level() == 4);
    assert (msg2.form() == "TEXT");
    assert (msg2.seqno() == 0);
    assert(!msg2.payload().empty());
    {
        std::string s(msg2.payload()[0].data<const char>(),msg2.payload()[0].size());
        assert(s == spl);
    }

    zio::json lobj{{"flow","EOT"},{"direction","extract"}};
    msg.set_label(lobj.dump());
    auto obj = zio::json::parse(msg.label());
    assert(obj["flow"] == "EOT");
    assert(obj["direction"] == "extract");
    obj["direction"] = "inject";
    zio::debug(msg.label());
    msg.set_label(obj.dump());
    obj = zio::json::parse(msg.label());
    assert(obj["direction"] == "inject");
    zio::debug(msg.label());

    {
        zio::Message empty;
        assert(empty.form().size() == 4);
        assert(empty.form() == "    ");
        assert(empty.prefix().dumps().substr(0,8) == "ZIO0    ");
        empty.set_form("FOO");
        assert(empty.form().size() == 4);
        assert(empty.form() == "FOO ");
        assert(empty.prefix().dumps().substr(0,8) == "ZIO0FOO ");
    }

    return 0;
}
