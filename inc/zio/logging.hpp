// fixme: replace this with logging with something global across ZIO.

#ifndef ZIO_LOGGING_HPP_SEEN
#define ZIO_LOGGING_HPP_SEEN

#include <string>

namespace zio {

    struct logbase_t {
        virtual ~logbase_t() {}
        virtual void debug(const std::string& msg) {}
        virtual void info(const std::string& msg) {}
        virtual void error(const std::string& msg) {}
    };
    struct console_log : public logbase_t  {
        enum class log_level : int {
            always=0, debug=1, info=2, error=3
        };
        log_level level{log_level::info};
        virtual ~console_log();
        virtual void debug(const std::string& msg);
        virtual void info(const std::string& msg);
        virtual void error(const std::string& msg);
        virtual void always(const std::string& msg, char lvl = 'A');
    };

}

#endif
