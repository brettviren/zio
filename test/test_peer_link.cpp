// basic peer discovery and presence with linking up

#include "zio/peer.hpp"
#include "zio/socket.hpp"

#include <iostream>
using namespace std;

int main(int argc, char* argv[])
{
    bool verbose=false;
    if (argc>1)
        verbose=true;

    // in a publisher application
    zio::Socket pub(ZMQ_PUB);
    const std::string addr = pub.bind("inproc://testpeerlink");
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
    zio::Socket sub(ZMQ_SUB);
    sub.subscribe();
    sub.connect(feeds[""]);

    // zmq wart: SSS, give time for pub to process any subscriptions.
    zclock_sleep(100);


    
    // back in publisher
    zstr_send(pub.zsock(), "Hello World!");
    if (verbose)
        cerr << "published\n";


    // back in subscriber
    char* resp = zstr_recv(sub.zsock());
    assert(resp);
    assert(streq(resp, "Hello World!"));
    if (verbose)
        cerr << resp <<endl;
    free (resp);
    
    return 0;
}
