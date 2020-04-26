#include "zio/port.hpp"
#include "zio/peer.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"

#include <iostream>
using namespace std;

int main()
{
    zio::init_all();

    // this test generally pretends to be the guts of a node.

    // create many ports
    zio::Port port("name1", ZMQ_PUB, "127.0.0.1");
    port.bind("127.0.0.1", 0);
    // port.bind("tcp://127.0.0.1:5678");
    // port.connect("name2", "portA");

    // bind all ports first, collecting their headers
    auto hh = port.do_binds();
    for (const auto& one : hh) {
        zio::debug("test: not advertising bind: {} = {}", one.first,
                   one.second);
    }

    // advertise those
    zio::Peer peer("test_port", hh);

    // tell all ports to go online and give the peer so they can
    // resovle any connects
    port.online(peer);

    // for completeness
    port.offline();

    return 0;
}
