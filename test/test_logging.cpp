#include "zio/logging.hpp"
#include "zio/main.hpp"

using namespace zio;

int main()
{
    zio::init_all();

    zio::trace("trace");        // avoid bare trace
    zio::debug("debug");        // avoid bare debug
    zio::info("info");
    zio::warn("warn");
    zio::error("error");
    zio::critical("critical");

    ZIO_TRACE("TRACE");         // prefer over bare call
    ZIO_DEBUG("DEBUG");         // prefer over bare call

    return 0;
}


