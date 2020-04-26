#include "zio/main.hpp"
#include "zio/util.hpp"
#include "zio/logging.hpp"

// this comes post 1.5.0
// #include <spdlog/cfg/env.h>
#include <cstdlib>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void zio::init_signals() { zio::catch_signals(); }

// until load_env release
static void load_env_levels()
{
    auto env_val =
        std::getenv("SPDLOG_LEVEL");  // only support a simple level name
    if (!env_val) { return; }
    spdlog::set_level(spdlog::level::from_str(env_val));
}

void zio::init_logging()
{
    // spdlog::cfg::load_env_levels();
    load_env_levels();
    auto console = spdlog::stdout_color_mt("console");
    auto err_logger = spdlog::stderr_color_mt("stderr");
    spdlog::set_default_logger(console);
}

void zio::init_all()
{
    zio::init_signals();
    zio::init_logging();
}
