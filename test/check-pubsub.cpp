/** Exercise ZeroMQ PUB/SUB or PUSH/PULL
 */

#include "zio/node.hpp"
#include "zio/main.hpp"
#include "zio/util.hpp"
#include "zio/stopwatch.hpp"
#include "zio/logging.hpp"
#include <fstream>
#include <string>
#include <vector>

std::string usage();

struct CountRate
{
    size_t ntocheck;       // check ever this number
    std::string name{""};  // what to call me
    size_t count{0};
    zio::Stopwatch sw;

    void operator()()
    {  // call each time something happens
        ++count;
        if (count and count % ntocheck == 0) {
            double dt_lap =
                std::chrono::duration_cast<std::chrono::milliseconds>(sw.lap())
                    .count();
            double dt_tot =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    sw.accum())
                    .count();
            zio::info("{}: rate: {:.4f} kHz, <{:.4f}> kHz", name,
                      ntocheck / dt_lap, count / dt_tot);
        }
    }
};

void do_source(zio::Node& node, zio::json& cfg)
{
    const size_t msg_size = cfg["size"].get<size_t>();
    const double msg_rate = cfg["rate"].get<double>();

    int64_t zzz_ms = 1000.0 / msg_rate;
    if (zzz_ms < 2) {
        zio::debug("source: message rate too fast, will free run");
        zzz_ms = 0;
    }
    else {
        zio::debug("source: message generation sleep: {} ms", zzz_ms);
    }

    const size_t nchirp = cfg["nchirp"].get<size_t>();
    CountRate cr{nchirp, "source"};

    std::vector<zio::socket_ref> socks;
    for (const auto& pname : node.portnames()) {
        auto port = node.port(pname);
        auto& sock = port->socket();
        int stype = zio::sock_type(sock);
        if (stype == ZMQ_PUB or stype == ZMQ_PUSH) { socks.push_back(sock); }
    }
    if (socks.empty()) { throw std::runtime_error("source given no PUBs"); }
    zio::info("source with {} PUBs", socks.size());

    std::vector<std::byte> buf(msg_size, std::byte(0));
    zio::const_buffer cbuf(buf.data(), buf.size());
    cr.sw.start();
    while (true) {
        if (zzz_ms) { zio::sleep_ms(zio::time_unit_t{zzz_ms}); }
        for (auto& sock : socks) {
            sock.send(cbuf, zio::send_flags::none);
            cr();
        }
    }  // run forever
}
void do_proxy(zio::Node& node, zio::json& cfg)
{
    const size_t nchirp = cfg["nchirp"].get<size_t>();
    CountRate cr{nchirp, "proxy"};

    std::vector<zio::socket_ref> pubs, subs;
    for (const auto& pname : node.portnames()) {
        auto port = node.port(pname);
        auto& sock = port->socket();
        int stype = zio::sock_type(sock);
        if (stype == ZMQ_SUB or stype == ZMQ_PULL) {
            subs.push_back(sock);
            if (stype == ZMQ_SUB) { port->subscribe(""); }
        }
        if (stype == ZMQ_PUB or stype == ZMQ_PUSH) { pubs.push_back(sock); }
    }
    if (subs.empty() or pubs.empty()) {
        throw std::runtime_error("proxy not given enough PUBs or SUBs");
    }

    const size_t nsubs = subs.size();
    const size_t npubs = pubs.size();
    zio::info("proxy with {} SUBs, {} PUBs", nsubs, npubs);

    zio::poller_t<> poller;
    for (auto& sock : subs) { poller.add(sock, zio::event_flags::pollin); }

    std::vector<zio::poller_event<>> events(nsubs);
    zio::message_t msg;
    cr.sw.start();
    while (true) {
        const int nevents = poller.wait_all(events, zio::time_unit_t{-1});
        for (int iev = 0; iev < nevents; ++iev) {
            auto res = events[iev].socket.recv(msg, zio::recv_flags::none);
            assert(res);  // we don't wait so this can never be false
            zio::const_buffer cbuf(msg.data(), msg.size());
            for (auto& pub : pubs) {
                pub.send(cbuf, zio::send_flags::none);
                cr();
            }
        }
    }  // run forever
}
void do_sink(zio::Node& node, zio::json& cfg)
{
    const size_t nchirp = cfg["nchirp"].get<size_t>();
    CountRate cr{nchirp, "sink"};

    std::vector<zio::socket_ref> socks;
    for (const auto& pname : node.portnames()) {
        auto port = node.port(pname);
        auto& sock = port->socket();
        int stype = zio::sock_type(sock);
        if (stype == ZMQ_SUB or stype == ZMQ_PULL) {
            socks.push_back(sock);
            if (stype == ZMQ_SUB) { port->subscribe(""); }
        }
    }
    if (socks.empty()) { throw std::runtime_error("sink given no SUBs"); }
    const size_t nsocks = socks.size();
    zio::info("sink with {} SUBs", socks.size());

    zio::poller_t<> poller;
    for (auto& sock : socks) { poller.add(sock, zio::event_flags::pollin); }

    std::vector<zio::poller_event<>> events(nsocks);
    zio::message_t msg;
    cr.sw.start();
    while (true) {
        const int nevents = poller.wait_all(events, zio::time_unit_t{-1});
        for (int iev = 0; iev < nevents; ++iev) {
            auto res = events[iev].socket.recv(msg, zio::recv_flags::none);
            assert(res);  // we don't wait so this can never be false
            cr();
        }
    }  // run forever
}

int main(int argc, char* argv[])
{
    zio::init_all();
    if (argc == 1) {
        zio::error(usage());
        return -1;
    }
    zio::debug("parsing config file {}", argv[1]);
    std::ifstream istr(argv[1]);
    zio::json cfg;
    istr >> cfg;

    std::function<void(zio::Node&, zio::json&)> proc;
    std::string type = cfg["type"].get<std::string>();
    if (type == "source") { proc = do_source; }
    else if (type == "proxy") {
        proc = do_proxy;
    }
    else if (type == "sink") {
        proc = do_sink;
    }
    else {
        throw std::runtime_error("unsupported type: " + type);
    }

    const std::string nodename = cfg["name"].get<std::string>();
    zio::debug("building node {}", nodename);
    zio::Node node(nodename);

    for (auto jport : cfg["ports"]) {
        int stype = jport["stype"].get<int>();
        std::string pname = jport["name"].get<std::string>();
        auto port = node.port(pname, stype);

        for (auto jep : jport["endpoints"]) {
            std::string link = jep["link"].get<std::string>();
            auto jaddr = jep["address"];

            if (link == "bind") {
                if (jaddr.is_string()) {
                    std::string addr = jaddr.get<std::string>();
                    if (addr.empty()) { port->bind(); }
                    else {
                        port->bind(addr);
                    }
                }
                else if (jaddr.is_array()) {
                    port->bind(jaddr[0].get<std::string>(),
                               jaddr[1].get<int>());
                }
                else {
                    throw std::runtime_error("unsupported bind address: " +
                                             jaddr.dump());
                }
            }
            else if (link == "connect") {
                if (jaddr.is_string()) {
                    std::string addr = jaddr.get<std::string>();
                    port->connect(addr);
                }
                else if (jaddr.is_array()) {
                    if (jaddr[1].is_string()) {
                        port->connect(jaddr[0].get<std::string>(),
                                      jaddr[1].get<std::string>());
                    }
                }
                else {
                    throw std::runtime_error("unsupported connect address: " +
                                             jaddr.dump());
                }
            }
            else {
                throw std::runtime_error("unsupported link type: " + link);
            }
        }
    }

    node.online();

    zio::debug("starting process {} with {}", type, cfg[type].dump());
    proc(node, cfg[type]);
    sleep(1);  // fake proc

    node.offline();
    return 0;
}

std::string usage()
{
    return R"V0G0N(

This one program implements exactly one of: a source, a sink or a
proxy.  Where PUB is mentioned, PUSH may be substituted, etc for
SUB/PULL.
 
- source :: generate messages of a given size and rate out a number
            of PUB sockets.

- sink :: consume messages from a number of SUB sockets.

- proxy :: a sink mated with a source.  Every message received on a
           SUB is sent to all PUBs.

It is fully configured by a JSON file providing an object with
these top-level attributes:

- name :: the node name used in peer discovery

- type :: "source", "sink" or "proxy"

- <type> :: the key "source", "sink" or "proxy" giving a TYPE-SPECIFC
            CONFIGURATION OBJECT

- ports :: array of PORT DESCRIPTION (see below)

The "source" TYPE-SPECIFIC CONFIGURATION OBJECT has these attributes:

- size :: message size in bytes

- rate :: message production rate

A PORT DESCRIPTION is an object with these attributes

- stype :: socket type as an integer using ZeroMQ numbering
- name :: port name for discovery, must be unique to the node
- endpoints :: a ENDPOINT DESCRIPTION or an array of them 

An ENDPOINT DESCRIPTION is an object that specifies a socket to
bind or connect.  It holds attributes:

- link :: "bind" or "connect"
- address :: an ADDRESS DESCRIPTION

An ADDRESS DESCRIPTION specifies a socket and can be in the
following forms:

- [string] :: a literal ADDRESS in ZeroMQ format: "tcp://a.b.c.d",
              "ipc://file.ipc".

- [tuple] :: a tuple of strings giving (nodename, portname) to connect
             which is resolved to a literal address with peer
             discovery or a string+integer (IP address, port number)
             to bind to a specific interface and port number.  Port
             may be 0 to be chosen ephemerally.

)V0G0N";
}
