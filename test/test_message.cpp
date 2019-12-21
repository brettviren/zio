#include "zio/message.hpp"

#include <cassert>

int main()
{
    zio::Message msg({{4,"TEXT","Secret Sauce"},{0xdeadbeaf,1234,0}});
    auto hh = msg.header();
    assert (hh.prefix.level == 4);
    assert (hh.prefix.format == "TEXT");
    assert(msg.payload().empty());

    auto raw = msg.encode();
    msg.decode(raw);
    assert (hh.prefix.level == 4);
    assert (hh.prefix.format == "TEXT");
    assert(msg.payload().empty());

    return 0;
}
