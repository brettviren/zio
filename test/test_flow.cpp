#include "zio/flow.hpp"
#include "zio/node.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"

using namespace std;

int main()
{
    zio::init_all();

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

    zio::debug("create flows");

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

    zio::debug("cflow send BOT");
    cflow.send_bot(msg);

    zio::debug("sflow recv BOT");
    ok = sflow.recv_bot(msg);
    assert(ok);
    zio::debug("sflow recv'ed");
    auto rid = msg.remote_id();
    zio::debug("sflow msg.label: {}, rid: {}", msg.label(), zio::binstr(rid));
    assert(rid.size());


    fobj = zio::json::parse(msg.label());
    std::string dir = fobj["direction"];
    assert (dir == "extract");
    fobj["direction"] = "inject";
    msg.set_label(fobj.dump());
    int credit = fobj["credit"];
    zio::debug("sflow credit: {}, rid: {}, stype:{}",
               credit, zio::binstr(rid), zio::sock_type(sport->socket()));

    assert (!sflow.is_sender());
    zio::debug("sflow msg.label: {}", msg.label());
    zio::debug("sflow send BOT");
    assert(msg.remote_id() == rid);
    sflow.send_bot(msg);
    zio::debug("cflow recv BOT");
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
    
    zio::debug("cflow send DAT");
    zio::json lobj{{"flow","DAT"}};
    msg.set_label(lobj.dump());
    ok = cflow.put(msg);
    assert(ok);
    assert(cflow.total_credit() - cflow.credit() == 1);

    zio::debug("sflow recv DAT, credit {}/{}",
               sflow.credit(), sflow.total_credit());
    assert(0 == sflow.credit());
    ok = sflow.get(msg);
    assert(ok);
    assert(sflow.credit() == 1);    
    
    zio::debug("sflow send EOT");
    sflow.send_eot(msg);
    zio::debug("cflow send EOT");
    ok = cflow.recv_eot(msg);
    assert(ok);
    cflow.send_eot(msg);
    sflow.recv_eot(msg);
    zio::debug("done");    
    return 0;
}
