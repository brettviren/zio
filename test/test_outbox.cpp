#include "zio/outbox.hpp"
#include "zio/node.hpp"
int main()
{
    zio::Node node("answer", 42);

    zio::Logger logger = node.logger("logs");
    zio::Metric metric = node.metric("metrics");

    logger(zio::level::info, "The play is the thing");
    metric(zio::level::info, {{"author","Shakespeare"}});

    logger.warning("Now is the time of our discontent");
    metric.error({{"timeline","strangest"}});

    return 0;
}
