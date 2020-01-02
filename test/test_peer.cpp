// basic peer discovery and presence

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


int main(int argc, char*argv[])
{
    bool verbose = false;
    if (argc > 1)
        verbose = true;

    zio::Peer peer1("peer1",{{"Color","blue"},{"Pet","cat"}}, verbose);

    std::string uuid2="";
    {
        zio::Peer peer2("peer2",{{"Color","yellow"},{"Pet","dog"}}, verbose);
        auto pids1 = peer2.waitfor("peer1");
        /// don't assert on this as "waf --alltests" makes parallel peers 
        //assert(pids1.size() == 1);
        auto pids2 = peer1.waitfor("peer2");
        //assert(pids2.size() == 1);
        uuid2 = pids2[0];
        assert(uuid2.size());
        bool found = peer1.isknown(uuid2);
        assert(found);
        dump_peer(peer1);
        dump_peer(peer2);
    }

    int countdown = 3;
    bool found;
    while (countdown) {
        cerr << "check for death of " << uuid2 << " " << countdown << endl;
        --countdown;
        peer1.poll(1000);
        found = peer1.isknown(uuid2);
        if (!found) { break; }
    }
    assert(!found);

    return 0;
}
