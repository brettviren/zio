#include "zio/flow.hpp"
#include "zio/node.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"
#include "zio/stopwatch.hpp"
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

    ZIO_DEBUG("[{} {}] socket: {}, giver: {}, credit: {}, timeout: {}",
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

    size_t ngive=0, ntake=0;
    zio::Stopwatch sw;
    sw.start();

    bool link_hit = false;
    while (true) {

        if (link_poller.wait_all(events, zio::time_unit_t{0})) {
            ZIO_DEBUG("[{} {}] link hit", nodename, portname);
            flow.eot();
            link_hit = true;
            break;
        }

        if (giver) {
            zio::Message msg;
            bool noto;
            try {
                noto = flow.put(msg);
            } catch (zio::flow::end_of_transmission) {
                ZIO_DEBUG("[{} {}] EOT during put(DAT)", nodename, portname);
                flow.eotack();
                break;
            }
            if (noto) {
                ZIO_TRACE("[{} {}] send DAT", nodename, portname);
                ++ngive;
            }
            else {
                ZIO_DEBUG("[{} {}] send DAT: TIMEOUT", nodename, portname);
            }
        }
        else {                  // taker
            zio::Message msg;
            bool noto;
            try {
                noto = flow.get(msg);
            } catch (zio::flow::end_of_transmission) {
                ZIO_DEBUG("[{} {}] EOT during put(DAT)", nodename, portname);
                flow.eotack();
                break;
            }
            if (noto) {
                ZIO_TRACE("[{} {}] recv DAT", nodename, portname);                
                ++ntake;
            }
            else {
                ZIO_DEBUG("[{} {}] recv DAT: TIMEOUT", nodename, portname);
            }
        }
    }

    sw.stop();
    auto khz_give = sw.hz(ngive)/1000.0;
    auto khz_take = sw.hz(ntake)/1000.0;

    zio::info("[{} {}] credit:{} gave:{} ({:.3f} kHz) took:{} ({:.3f} kHz)",
              nodename, portname, credit, ngive, khz_give, ntake, khz_take);

    ZIO_DEBUG("[{} {}] node going offline", nodename, portname);
    node.offline();

    if (!link_hit) {
        ZIO_DEBUG("[{} {}] waiting for actor shutdown", nodename, portname);
        zio::message_t rmsg;
        auto res = link.recv(rmsg);
        assert(res);
    }
}

#include "zio/actor.hpp"

void test_flow(int credit)
{
    zio::context_t ctx;

    ZIO_DEBUG("test_flow: start actors with {} credit", credit);

    zio::zactor_t one(ctx, flow_endpoint, ZMQ_SERVER, false, credit);
    zio::zactor_t two(ctx, flow_endpoint, ZMQ_CLIENT, true,  credit);

    ZIO_DEBUG("test_flow: sleep");
    zio::sleep_ms(zio::time_unit_t{1000});
    ZIO_DEBUG("test_flow: shutdown actors");
    zio::message_t signal;
    one.link().send(signal, zio::send_flags::none);
    two.link().send(signal, zio::send_flags::none);
    ZIO_DEBUG("test_flow: sleep again");
    zio::sleep_ms(zio::time_unit_t{100});
    ZIO_DEBUG("test_flow: exit");
}



int main()
{
    zio::init_all();
    test_flow(10);
    test_flow(5);
    test_flow(2);
    test_flow(1);

    return 0;
}
