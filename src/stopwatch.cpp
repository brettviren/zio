#include "zio/stopwatch.hpp"

zio::Stopwatch::Stopwatch()
{
    dt_accum = duration::zero();
}

zio::Stopwatch::time_point zio::Stopwatch::now()
{
    return clock::now();
}

zio::Stopwatch::time_point zio::Stopwatch::start()
{
    started = true;
    t_start = t_lap = now();
    return t_start;
}

zio::Stopwatch::duration zio::Stopwatch::stop()
{
    if (!started) {
        return duration::zero();
    }
    auto t_now = now();
    dt_accum += t_now - t_start;
    started = false;
    return dt_accum;
}

zio::Stopwatch::duration zio::Stopwatch::lap()
{
    if (!started) {
        return duration::zero();
    }
    auto t_now = now();
    auto dt = t_now - t_lap;
    t_lap = t_now;
    return dt;
}

zio::Stopwatch::duration zio::Stopwatch::accum()
{
    if (!started) {
        return dt_accum;
    }
    auto t_now = now();
    return dt_accum + (t_now - t_start);
}

