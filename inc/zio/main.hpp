#ifndef ZIO_MAIN_HPP_SEEN
#define ZIO_MAIN_HPP_SEEN

namespace zio {

    // Arrange to catch signals.
    void init_signals();

    // Initialize logging;
    void init_logging();

    // Initialize signal catching and logging.
    void init_all();
}

#endif

