#include "zio/outbox.hpp"
#include "zio/node.hpp"
#include "zio/senders.hpp"
int main()
{
    zio::Node node("answer", 42);
    node.set_verbose(true);

    zio::Logger logger(zio::TextSender(node.port("logs", ZMQ_PUSH)));
    zio::Metric metric(zio::JsonSender(node.port("metrics", ZMQ_PUSH)));
    node.port("logs")->bind();
    node.port("metrics")->bind();
    node.online();

    zio::Node other("question", 22);
    other.set_verbose(true);
    auto logp = other.port("logsink", ZMQ_PULL);
    logp->connect("answer","logs");
    auto metp = other.port("metsink", ZMQ_PULL);
    metp->connect("answer","metrics");
    // p->subscribe("ZIO");
    other.online();
    

    logger(zio::level::info, "The play");
    logger(zio::level::info, "is the thing");
    metric(zio::level::info, {{"author","Shakespeare"}});

    logger.warning("Now is the time of our discontent");
    metric.error({{"timeline","strangest"}});

    zsys_debug("Now, receive three LOGs");
    zio::Message msg;
    for (int ind=0; ind<3; ++ind) {
        bool ok  = logp->recv(msg);
        assert(ok);

        auto hdr = msg.header();
        zsys_debug("\t%d %d", ind, hdr.coord.seqno);
        assert(hdr.coord.seqno == ind+1);
    }
          

    return 0;
}
