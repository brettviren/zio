#include "zio/peer.hpp"

#include <iostream>
using namespace std;

void dump_peer(zio::Peer& peer) {
    cerr << "peer '" << peer.nickname() << "' sees:" << endl;

    for (auto pi : peer.peers()) {
        cerr << "\t" << pi.first << " is " << pi.second.nick << endl;
        for (auto h : pi.second.headers) {
            cerr << "\t\t" << h.first << " = " << h.second << endl;
        }
    }
}


int main()
{
    zio::Peer peer1("peer1",{{"Color","blue"},{"Pet","cat"}});
    zio::Peer peer2("peer2",{{"Color","yellow"},{"Pet","dog"}});

    auto pids1 = peer2.waitfor("peer1");
    assert(pids1.size() == 1);
    auto pids2 = peer1.waitfor("peer2");
    assert(pids2.size() == 1);
    dump_peer(peer1);
    dump_peer(peer2);


    return 0;
}
