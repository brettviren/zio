#include "zio/logging.hpp"

using namespace zio;

int main()
{
    console_log log;
    //log.level = console_log::log_level::debug;
    log.debug("debug");
    log.info("info");
    log.error("error");
    return 0;
}


