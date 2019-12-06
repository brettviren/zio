#include "zio/outbox.hpp"
#include "zio/node.hpp"
int main()
{
    zio::Node node("answer", 42);
    auto lp = node.port("logs", ZMQ_PUB);
    auto mp = node.port("metrics", ZMQ_PUB);

    zio::Logger logger(lp);
    zio::Metric metric(mp);

    logger(zio::level::info, "The play is the thing");
    metric(zio::level::info, {{"author","Shakespeare"}});

    logger.warning("Now is the time of our discontent");
    metric.error({{"timeline","strangest"}});

    return 0;
}
