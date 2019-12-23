#include "zio/node.hpp"
#include "zio/outbox.hpp"
#include "zio/senders.hpp"
#include "zio/format.hpp"

void test_it(int sender_stype, int recver_stype)
{
    zio::Node sender("sender",1);
    sender.set_verbose();
    auto po = sender.port("outbox", sender_stype);
    po->bind("inproc://endtoend");
    //po->connect("recver","inbox");
    sender.online();

    zio::Node recver("recver",2);
    recver.set_verbose();
    auto pi = recver.port("inbox", recver_stype);
    //pi->bind();
    pi->connect("sender","outbox");
    if (recver_stype == ZMQ_SUB) {
        pi->subscribe("ZIO");
    }
    recver.online();


    zio::Logger log(zio::TextSender(sender, "outbox"));

    // zmq wart: SSS, give time for pub to process any subscriptions.
    //zclock_sleep(100);
    
    zsys_debug("test_endtoend: sending log message");
    log.info("Hello world!");

    zio::converter::text_t tc;

    zio::Message msg;
    zsys_debug("test_endtoend: receiving");
    bool ok = pi->recv(msg, 1000);
    assert(ok);

    std::string line = tc(msg.payload()[0]);
    assert(line == "Hello world!");
    auto header = msg.header();
    assert(header.coord.seqno == 1);
    assert(header.coord.origin == 1);
    zsys_debug("header.prefix.label size %d", header.prefix.label.size());
    assert(header.prefix.label.empty());
    assert(header.prefix.level == zio::level::info);
}

int main()
{
    test_it(ZMQ_PUB, ZMQ_SUB);
    zsys_debug("PUB/SUB complete");

    test_it(ZMQ_PUSH, ZMQ_PULL);
    zsys_debug("PUSH/PULL complete");

    test_it(ZMQ_CLIENT, ZMQ_SERVER);
    zsys_debug("CLIENT/SERVER complete");

    return 0;    
}
