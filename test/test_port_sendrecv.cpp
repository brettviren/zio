#include "zio/node.hpp"
#include "zio/message.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"
int main()
{
    zio::init_all();

    zio::Node node("test-port-send-recv");
    auto p1 = node.port("sender", ZMQ_SERVER);
    auto p2 = node.port("recver", ZMQ_CLIENT);
    p1->bind("tcp://127.0.0.1:5555");
    p2->connect("tcp://127.0.0.1:5555");
    node.online();

    {
        zio::Message msg("TEXT");
        msg.set_label("label");
        msg.set_seqno(1);
        p2->send(msg);
    }

    {
        zio::Message msg2;
        bool ok = p1->recv(msg2);
        assert(ok);
        assert(msg2.label() == "label");
        assert(msg2.seqno() == 1);
        zio::debug("Got remote id {}", zio::binstr(msg2.remote_id()));
        assert(msg2.remote_id().size() > 0);

        msg2.set_seqno(2);
        
        p1->send(msg2);
    }
    {
        
        zio::Message msg3;
        p2->recv(msg3);
        assert(msg3.seqno() == 2);
        assert(msg3.label() == "label");
    }
    node.offline();
    return 0;
}
