#include "zio/logging.hpp"
#include "zio/main.hpp"

using namespace zio;

int main()
{
    zio::init_all();

    // You won't see debug unless do like:
    // $ SPDLOG_LEVEL=debug ./build/test_logging
    zio::debug("debug");
    zio::info("info");
    zio::warn("warn");
    zio::error("error");
    zio::critical("critical");

    return 0;
}


