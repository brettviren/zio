#include "zio/flow.hpp"
#include "zio/node.hpp"

#include <iostream>

using namespace std;

int main()
{

    zio::Node snode("server", 1);
    snode.set_verbose();
    auto sport = snode.port("recver", ZMQ_SERVER);
    sport->bind("inproc://testflow");
    snode.online();

    zio::Node cnode("client", 1);
    cnode.set_verbose();
    auto cport = cnode.port("client", ZMQ_CLIENT);
    cport->connect("inproc://testflow");
    cnode.online();


    cerr << "create flows" << endl;

    zio::flow::Flow sflow(sport);
    zio::flow::Flow cflow(cport);

    zio::Message msg;
    msg.set_format("FLOW");

    zio::json fobj = {{"flow","BOT"},{"credits",2},{"direction","extract"}};
    msg.set_label(fobj.dump());

    bool ok;

    cerr << "cflow send BOT" << endl;
    cflow.send_bot(msg);

    cerr << "sflow recv BOT" << endl;
    ok = sflow.recv_bot(msg);
    assert(ok);
    auto rid = msg.routing_id();
    cerr << "cflow msg.label: " << msg.label() << " rid: " << rid <<  endl;
    assert(rid);


    fobj = zio::json::parse(msg.label());
    std::string dir = fobj["direction"];
    assert (dir == "extract");
    fobj["direction"] = "inject";
    msg.set_label(fobj.dump());
    int credits = fobj["credits"];
    cerr <<"sflow credits:"<<credits << " rid:"<< rid
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

    cerr <<"cflow send DAT\n";
    zio::json lobj{{"flow","DAT"}};
    msg.set_label(lobj.dump());
    ok = cflow.put(msg);
    assert(ok);
    assert(cflow.total_credits() - cflow.credits() == 1);

    cerr <<"sflow recv DAT\n";
    assert(sflow.total_credits() == sflow.credits());
    ok = sflow.get(msg);
    assert(ok);
    assert(sflow.credits() == 1);    
    
    cerr <<"sflow send EOT\n";
    sflow.eot(msg, 0);
    cerr <<"cflow send EOT\n";
    ok = cflow.eot(msg);
    assert(ok);
    cerr<<"done\n";    
    return 0;
}
