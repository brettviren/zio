#include "zio/flow.hpp"
#include "zio/node.hpp"
#include <random>

using namespace std;

int main()
{
    zsys_init();

    zio::Node cnode("client", 2);
    cnode.set_verbose();
    auto cport = cnode.port("sender", ZMQ_CLIENT);
    cport->connect("testflows","recver");
    cnode.online();

    zsys_debug("create flows");

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

    zsys_debug("cflow recv BOT");
    ok = cflow.recv_bot(msg);
    assert(ok);

    assert (cflow.is_sender());

    
    std::default_random_engine rng;
    std::uniform_real_distribution<double> maybe(0,1);

    bool I_quit = false;
    while (true) {
        zsys_debug ("cflow send DAT");
        zio::json lobj{{"flow","DAT"}};
        msg.set_label(lobj.dump());
        ok = cflow.put(msg);
        if (!ok) {
            zsys_debug("false return, maybe EOT");
            break;
        }
        zsys_debug("have credits %d/%d",
                   cflow.credits(),
                   cflow.total_credits() );
        
        //assert(cflow.total_credits() - cflow.credits() == 1);
        if (maybe(rng) > 0.9) {
            I_quit = true;
            break;
        }
    }
    if (I_quit) {
        zsys_debug("cflow send EOT");
        ok = cflow.eot(msg,-1);
        assert(ok);
    }
    else {
        zsys_debug("cflow recv EOT");
        ok = cflow.eot(msg,0);
        assert(ok);
    }
    return 0;
}
