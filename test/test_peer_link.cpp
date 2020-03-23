// basic peer discovery and presence with linking up

#include "zio/peer.hpp"
#include "zio/util.hpp"

#include <iostream>
using namespace std;

int main(int argc, char* argv[])
{
    bool verbose=false;
    if (argc>1)
        verbose=true;

    zio::context_t ctx;

    // in a publisher application
    zio::socket_t pub(ctx, ZMQ_PUB);
    const std::string addr = "inproc://testpeerlink";
    pub.bind(addr);
    zio::Peer pubpeer("pub",{{"Feed",addr}}, verbose);

    // in a subscriber application
    zio::Peer subpeer("sub", {}, verbose);
    auto uuids = subpeer.waitfor("pub");
    assert (uuids.size() == 1);
    if (verbose)
        cerr << "pub uuid is " << uuids[0] << endl;
    auto pubinfo = subpeer.peer_info(uuids[0]);
    if (verbose)
        cerr << "pub nick is " << pubinfo.nick << endl;
    assert (pubinfo.nick == "pub");
    assert (pubinfo.headers.size() == 1);
    auto feeds = pubinfo.branch("Feed");
    if (verbose)
        cerr << "got " << feeds.size() << ": " << feeds[""] << endl;
    assert(feeds.size() == 1);
    zio::socket_t sub(ctx, ZMQ_SUB);
    std::string prefix = "";
    sub.setsockopt(ZMQ_SUBSCRIBE, prefix.c_str(), prefix.size());

    sub.connect(feeds[""]);

    // zmq wart: SSS, give time for pub to process any subscriptions.
    zclock_sleep(100);


    std::string hw = "Hello World!";
    // back in publisher
    pub.send(zio::buffer(hw.data(), hw.size()));
    if (verbose)
        cerr << "send "<<hw<<"\n";


    // back in subscriber
    zio::message_t msg;
    auto res = sub.recv(msg);
    assert(res);
    std::string hw2(static_cast<char*>(msg.data()), msg.size());
    assert(hw2 == hw);
    if (verbose)
        cerr << "recv "<<hw2<<"\n";


    
    return 0;
}
