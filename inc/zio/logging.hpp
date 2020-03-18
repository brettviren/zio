#ifndef ZIO_LOGGING_HPP_SEEN
#define ZIO_LOGGING_HPP_SEEN

#include <spdlog/spdlog.h>

namespace zio {

    // Bring defalt loggers into zio::

    using spdlog::debug;
    using spdlog::info;
    using spdlog::warn;
    using spdlog::error;
    using spdlog::critical;

}

#endif
