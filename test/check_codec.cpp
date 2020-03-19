/** Provide one end of a check of message codec.  
 *
 * See "zio check-codec" for a Python equivallent
 */

#include "zio/cppzmq.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"
#include "zio/node.hpp"
#include "zio/message.hpp"

const std::string greeting = "Hello World! "; // 13
const size_t spaminess = 5;                   // number of doubling

std::string spamify(std::string spam, size_t factor)
{
    for (int ind=0; ind<factor; ++ind) {
        spam += spam;
    }
    return spam;
}

static
int compare_message(const zio::Message& m1, const zio::Message& m2)
{
    // This is not a general comparison
    assert(m1.form() == m2.form());
    auto mp1 = m1.toparts();
    auto mp2 = m2.toparts();
    assert(mp1.size() == mp1.size());
    assert(mp1.size() == 4);     // two headers, two payloads
    for (size_t ind=0; ind<4; ++ind) {
        auto& p1 = mp1[ind];
        auto& p2 = mp2[ind];
        zio::debug("compare: part {}: size1: {} size2: {}", ind, p1.size(), p2.size());
        assert(p1.size() == p2.size());
        if (ind == 1) {
            continue;           // coord header is expected to differ
        }
        assert(0 == memcmp(p1.data(), p2.data(), p1.size()));
    }
    return 0;
}

static
int do_clientish(int stype, std::string address)
{
    zio::Node node("check-codec-clientish");
    auto port = node.port("client", stype);
    port->connect(address);
    node.online();

    zio::Message msg("TEXT");

    std::string spam = greeting;
    msg.add({spam.data(), spam.size()});

    spam = spamify(greeting, spaminess);
    msg.add({spam.data(), spam.size()});

    zio::debug("clientish sending");
    port->send(msg);

    zio::debug("clientish receiving");
    zio::Message msg2;
    port->recv(msg2);

    return compare_message(msg, msg2);
}

int do_serverish(int stype, std::string address)
{
    zio::Node node("check-codec-serverish");
    auto port = node.port("server", stype);
    port->bind(address);
    node.online();

    zio::debug("serverish receiving");

    zio::Message msg;
    port->recv(msg);

    auto mp = msg.toparts();
    assert (mp.size() == 4);

    std::string spam = greeting;
    assert (mp[2].to_string() == spam);
    spam = spamify(greeting, spaminess);
    assert (mp[3].to_string() == spam);

    zio::debug("serverish part 0: {}", mp[0].to_string());
    zio::debug("serverish part 2: {}", mp[2].to_string());
    zio::debug("serverish part 3: {}", mp[3].to_string());

    zio::debug("serverish sending");
    port->send(msg);
    return 0;
}

int main(int argc, char* argv[])
{
    zio::init_all();

    std::string which = "server";
    if (argc > 1) {
        which  = argv[1];
    }

    std::string address = "tcp://127.0.0.1:5555";
    if (argc > 2) {
        address = argv[2];
    }

    zio::info("{} on {}", which, address);

    if (which == "server") {
        return do_serverish(ZMQ_SERVER, address);
    }
    if (which == "router") {
        return do_serverish(ZMQ_ROUTER, address);
    }
    if (which == "client") {
        return do_clientish(ZMQ_CLIENT, address);
    }
    if (which == "dealer") {
        return do_clientish(ZMQ_DEALER, address);
    }
    return -1;
}
