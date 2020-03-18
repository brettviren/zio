#include "zio/main.hpp"
#include "zio/util.hpp"
#include "zio/logging.hpp"

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void zio::init_signals()
{
    zio::catch_signals();
}

void zio::init_logging()
{
    spdlog::cfg::load_env_levels();
    auto console = spdlog::stdout_color_mt("console");    
    auto err_logger = spdlog::stderr_color_mt("stderr");    
    spdlog::set_default_logger(console);
}

 
void zio::init_all()
{
    zio::init_signals();
    zio::init_logging();
}
