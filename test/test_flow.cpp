#include "zio/flow.hpp"
#include "zio/node.hpp"

#include <iostream>

using namespace std;

int main()
{
    zsys_init();

    zio::Node snode("server", 1);
    snode.set_verbose();
    auto sport = snode.port("recver", ZMQ_SERVER);
    // C/S can't do inproc!
    /// const char* addr = "inproc://testflow";
    // avoid testing with tcp as we tend to use the same ports in multiple tests
    /// const char* addr = "tcp://127.0.0.1:5678";
    // goldilocks
    const char* addr = "ipc://testflow";
    sport->bind(addr);
    snode.online();

    zio::Node cnode("client", 2);
    cnode.set_verbose();
    auto cport = cnode.port("sender", ZMQ_CLIENT);
    cport->connect(addr);
    cnode.online();

    zsys_debug("create flows");

    // flow normally acts as a client but can act as a server
    zio::flow::Flow sflow(sport);
    zio::flow::Flow cflow(cport);

    zio::Message msg("FLOW");

    const int credits_in_play = 2;

    zio::json fobj = {{"flow","BOT"},
                      {"credit",credits_in_play},
                      {"direction","extract"}};
    msg.set_label(fobj.dump());

    bool ok;

    zsys_debug("cflow send BOT");
    cflow.send_bot(msg);

    zsys_debug("sflow recv BOT");
    ok = sflow.recv_bot(msg);
    assert(ok);
    zsys_debug("sflow recv'ed");
    auto rid = msg.routing_id();
    zsys_debug("sflow msg.label: %s, rid: %d", msg.label().c_str(), rid);
    assert(rid);


    fobj = zio::json::parse(msg.label());
    std::string dir = fobj["direction"];
    assert (dir == "extract");
    fobj["direction"] = "inject";
    msg.set_label(fobj.dump());
    int credit = fobj["credit"];
    cerr <<"sflow credit:"<<credit << " rid:"<< rid
         <<" stype:" << zio::sock_type(sport->socket()) << endl;

    assert (!sflow.is_sender());
    cerr <<"sflow msg.label: " <<  msg.label() << endl;
    cerr << "sflow send BOT\n";
    assert(msg.routing_id() == rid);
    sflow.send_bot(msg);
    cerr << "cflow recv BOT\n";
    ok = cflow.recv_bot(msg);
    assert(ok);

    assert (cflow.is_sender());

    // at this point deadlock could occur because this test is
    // synchronous between both client and server.  We must manually
    // prime the pump.  Internally the server's get() would do this.
    credit = sflow.flush_pay();
    assert (credit == credits_in_play);
    assert (credits_in_play == sflow.total_credit());
    assert (0 == sflow.credit());
    
    cerr <<"cflow send DAT\n";
    zio::json lobj{{"flow","DAT"}};
    msg.set_label(lobj.dump());
    ok = cflow.put(msg);
    assert(ok);
    assert(cflow.total_credit() - cflow.credit() == 1);

    zsys_debug("sflow recv DAT, credit %d/%d",
               sflow.credit(), sflow.total_credit());
    assert(0 == sflow.credit());
    ok = sflow.get(msg);
    assert(ok);
    assert(sflow.credit() == 1);    
    
    cerr <<"sflow send EOT\n";
    sflow.send_eot(msg);
    cerr <<"cflow send EOT\n";
    ok = cflow.recv_eot(msg);
    assert(ok);
    cflow.send_eot(msg);
    sflow.recv_eot(msg);
    cerr<<"done\n";    
    return 0;
}
