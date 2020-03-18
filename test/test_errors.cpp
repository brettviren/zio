#include "zio/interned.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"

static
void test_socket()
{
    zio::init_all();
    zio::context_t ctx;
    zio::socket_t s(ctx, ZMQ_PUB);
    try {
        s.bind("wrong");
    }
    catch (zio::error_t& se) {
        zio::debug("Correctly caught {}", se.what());
    }
    try {
        s.connect("wrong");
    }
    catch (zio::error_t& se) {
        zio::debug("Correctly caught {}", se.what());
    }
}

int main()
{
    test_socket();
    return 0;
}
