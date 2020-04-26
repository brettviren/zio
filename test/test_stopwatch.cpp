#include "zio/stopwatch.hpp"
#include "zio/util.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"

int main()
{
    zio::init_all();

    const auto zero = zio::Stopwatch::duration::zero();

    zio::Stopwatch sw;
    const auto mystart = sw.now();

    assert(sw.stop() == zero);
    assert(sw.lap() == zero);
    assert(sw.accum() == zero);

    assert(sw.start() >= mystart);
    {
        zio::sleep_ms(std::chrono::milliseconds{1000});
        auto dt = sw.stop();
        auto dt_us =
            std::chrono::duration_cast<std::chrono::microseconds>(dt).count();
        zio::debug("1 second sleep gives {}us", dt_us);
        assert(std::abs(dt_us - 1000000) < 10000);
    }

    assert(sw.start() >= mystart);
    {
        zio::sleep_ms(std::chrono::milliseconds{1000});
        auto dt = sw.stop();
        auto dt_us =
            std::chrono::duration_cast<std::chrono::microseconds>(dt).count();
        zio::debug("1+1 second sleep gives {}us", dt_us);
        assert(std::abs(dt_us - 2000000) < 10000);
    }

    {
        auto tot = sw.accum();
        auto tot_us =
            std::chrono::duration_cast<std::chrono::microseconds>(tot).count();
        zio::debug("1+1 second sleep accumulates to {}us", tot_us);
        assert(std::abs(tot_us - 2000000) < 10000);
    }
    return 0;
}
