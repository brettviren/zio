#include <chrono>
namespace zio {

    /*! Use to mark how fast something goes */
    class Stopwatch {
    public:

        typedef std::chrono::high_resolution_clock clock;
        typedef clock::time_point time_point;
        typedef clock::duration duration;

        /// Create a stopwatch.  It will not yet be "running".
        Stopwatch();

        /// The current time, doesn't affect the stopwatch.
        time_point now();

        /// Start the stop watch, return the starting time ("now")
        time_point start();

        /// Stop the stop watch, return the elapsed time from start
        duration stop();

        /// Return time since last lap (or full) start and set current lap start.
        duration lap();

        /// Return total accumulated time when stopwatch was running.
        duration accum();

        /// Return an average rate in Hz if count things happened over accum
        double hz(size_t count);

    private:
        bool started{false};
        time_point t_start, t_lap;
        duration dt_accum;
        
    };
}
