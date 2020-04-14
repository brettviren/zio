#ifndef ZIO_LOGGING_HPP_SEEN
#define ZIO_LOGGING_HPP_SEEN

#include <spdlog/spdlog.h>

namespace zio {

    // Bring defalt loggers into zio::

    using spdlog::trace;
    using spdlog::debug;
    using spdlog::info;
    using spdlog::warn;
    using spdlog::error;
    using spdlog::critical;


}

// Compile-time knock-outs.  Again, just wrappers/copies of
// spdlog.  We keep the compile time macros.
    
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
#define ZIO_TRACE(...) SPDLOG_LOGGER_TRACE(spdlog::default_logger_raw(), __VA_ARGS__)
#else
#define ZIO_TRACE(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
#define ZIO_DEBUG(...) SPDLOG_LOGGER_DEBUG(spdlog::default_logger_raw(), __VA_ARGS__)
#else
#define ZIO_DEBUG(...) (void)0
#endif


#endif
