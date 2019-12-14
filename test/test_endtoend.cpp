#include "zio/node.hpp"
#include "zio/outbox.hpp"
#include "zio/format.hpp"

void test_it(int sender_stype, int recver_stype)
{
    zio::Node sender("sender",1);
    sender.set_verbose();
    auto po = sender.port("outbox", sender_stype);
    po->bind();
    sender.online();
    zio::Logger log(po);

    zio::Node recver("recver",2);
    recver.set_verbose();
    auto pi = recver.port("inbox", recver_stype);
    pi->connect("sender","outbox");
    if (recver_stype == ZMQ_SUB) {
        pi->subscribe("ZIO");
    }
    recver.online();

    // zmq wart: SSS, give time for pub to process any subscriptions.
    zclock_sleep(100);
    
    zsys_debug("test_endtoend: sending log message");
    log.info("Hello world!");

    zio::converter::text_t tc;

    zio::Header header;
    zio::byte_array_t buffer;
    zsys_debug("test_endtoend: receiving");
    int rc = pi->recv(header, buffer);
    zsys_debug("test_endtoend: received (rc=%d)", rc);
    assert(rc == 0);
    std::string line = tc(buffer);
    assert(line == "Hello world!");
    assert(header.seqno == 0);
    assert(header.origin == 1);
    assert(header.label == "");
    assert(header.level == zio::level::info);
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
