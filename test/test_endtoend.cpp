#include "zio/node.hpp"
#include "zio/outbox.hpp"
#include "zio/format.hpp"

int main()
{
    zio::Node sender("sender",1);
    sender.set_verbose();
    auto po = sender.port("outbox", ZMQ_PUB);
    po->bind();
    sender.online();
    zio::Logger log(po);

    zio::Node recver("recver",2);
    recver.set_verbose();
    auto pi = recver.port("inbox", ZMQ_SUB);
    pi->connect("sender","outbox");
    pi->subscribe("ZIO");
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

    return 0;    
}
