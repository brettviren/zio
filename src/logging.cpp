// fixme: this is a garbage place holder for something like spdlog.

#include "zio/logging.hpp"
#include <cstdio>
#include <sstream>

using namespace zio;

console_log::~console_log() {}


void console_log::debug(const std::string& msg)
{
    if (level > log_level::debug) { return; }
    always(msg,'D');
}

void console_log::info(const std::string& msg)
{
    if (level > log_level::info) { return; }
    always(msg,'I');
}

void console_log::error(const std::string& msg)
{
    if (level > log_level::error) { return; }
    always(msg,'E');
}

void console_log::always(const std::string& msg, char lvl)
{
    time_t curtime = time (NULL);
    struct tm *loctime = localtime (&curtime);
    char formatted[20] = {0};
    strftime (formatted, 20, "%y-%m-%d %H:%M:%S ", loctime);
    printf ("%s", formatted);
    printf("%c: %s\n", lvl, msg.c_str());
}
