#include "zio/flow.hpp"
#include "zio/node.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"
#include "zio/actor.hpp"

static
void flow_endpoint(zio::socket_t& link, int socket, bool giver, int credit)
{
    // actor ready
    link.send(zio::message_t{}, zio::send_flags::none);

    // create node based on socket/giver config
    std::string nodename = "client";
    std::string othernode = "server";
    if (socket == ZMQ_SERVER or socket == ZMQ_ROUTER) {
        nodename = "server";
        othernode = "client";
    }
    zio::flow::direction_e direction = zio::flow::direction_e::inject;
    std::string portname = "taker";
    std::string otherport = "giver";
    if (giver) {
        direction = zio::flow::direction_e::extract;
        portname = "giver";
        otherport = "taker";
    }
    // tack on "c" or "s" to make logs hint what role a flow/port has.
    portname += nodename[0];
    otherport += othernode[0];

    zio::timeout_t timeout{1000};

    zio::debug("[{} {}] socket: {}, giver: {}, credit: {}, timeout: {}",
               nodename, portname, zio::sock_type_name(socket),
               giver, credit, timeout.value().count());

    zio::Node node(nodename);
    auto port = node.port(portname, socket);
    if (nodename == "server") { // note, client okay to bind too
        port->bind();           // but keep it simple here.
    }                           
    else {
        port->connect(othernode, otherport);
    }
    node.online();

    zio::Flow flow(port, direction, credit);

    flow.bot();

    zio::poller_t<> link_poller;
    link_poller.add(link, zio::event_flags::pollin);

    std::vector<zio::poller_event<>> events(1);

    while (true) {

        if (link_poller.wait_all(events, zio::time_unit_t{0})) {
            zio::debug("[{} {}] link hit", nodename, portname);
            flow.eot();
            break;
        }

        if (giver) {
            zio::Message msg;
            bool noto = flow.put(msg);
            if (noto) {
                zio::debug("[{} {}] send DAT", nodename, portname);
            }
            else {
                zio::debug("[{} {}] send DAT: TIMEOUT", nodename, portname);
            }
        }
        else {                  // taker
            zio::Message msg;
            bool noto = flow.get(msg);
            if (noto) {
                zio::debug("[{} {}] recv DAT", nodename, portname);                
            }
            else {
                zio::debug("[{} {}] recv DAT: TIMEOUT", nodename, portname);
            }
        }
    }

    zio::debug("[{} {}] node going offline", nodename, portname);
    node.offline();

    zio::debug("[{} {}] waiting for actor shutdown", nodename, portname);
    zio::message_t rmsg;
    auto res = link.recv(rmsg);
    assert(res);
}

#include "zio/actor.hpp"

void test_flow()
{
    zio::context_t ctx;

    const int credit = 10;

    zio::debug("test_flow: start actors with {} credit", credit);

    zio::zactor_t one(ctx, flow_endpoint, ZMQ_SERVER, false, credit);
    zio::zactor_t two(ctx, flow_endpoint, ZMQ_CLIENT, true,  credit);

    zio::debug("test_flow: sleep");
    zio::sleep_ms(zio::time_unit_t{1000});
    zio::debug("test_flow: shutdown actors");
    zio::message_t signal;
    one.link().send(signal, zio::send_flags::none);
    two.link().send(signal, zio::send_flags::none);
    zio::debug("test_flow: sleep again");
    zio::sleep_ms(zio::time_unit_t{100});
    zio::debug("test_flow: exit");
}


int main()
{
    zio::init_all();
    test_flow();
    return 0;
}
