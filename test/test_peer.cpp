#include "zio/peer.hpp"

#include <iostream>

using namespace std;
int main()
{
    zio::Peer peer1("peer1",{{"Color","blue"},{"Pet","cat"}});
    cerr << "peer1" << endl;
    zio::Peer peer2("peer2",{{"Color","yellow"},{"Pet","dog"}});
    cerr << "peer2" << endl;

    auto pids = peer2.waitfor("peer1");
    assert(pids.size() == 1);
    cerr << pids[0] << endl;

    return 0;
}
