/*! cppzmq does not directly provide an actor pattern.
 * this tests a simple one. */


#include "zio/actor.hpp"
#include "zio/logging.hpp"
#include "zio/main.hpp"

#include <thread>
#include <sstream>

#include <chrono>


static
void myactor(zio::socket_t& link, std::string greeting, bool fast_exit)
{
    zio::debug("myactor: {}", greeting);

    // ready
    link.send(zio::message_t{}, zio::send_flags::none);

    zio::message_t msg;
    zio::debug("myactor: wait for app protocol message");
    auto res1 = link.recv(msg);
    assert(res1);
    zio::debug("myactor: got protocol message size {}", msg.size());
    assert(msg.size() == 2);
    assert(msg.to_string() == "hi");

    if (fast_exit) {
        zio::debug("myactor: exit early");
        return;
    }

    zio::debug("myactor: simulating 1 second of work, try to Ctrl-c me");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));


    zio::debug("myactor: waiting for termination message");
    zio::message_t rmsg;
    auto res2 = link.recv(rmsg);
    assert(res2);
    assert(rmsg.to_string() == "$TERM");
    zio::debug("myactor: exiting");
}


int main()
{
    zio::init_all();

    zio::context_t ctx;

    zio::debug("in main, test 1");
    {
        zio::zactor_t actor(ctx, myactor, "hello world", false);
        zio::debug("1 in main, sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        zio::debug("1 in main, send protocol message");
        actor.link().send(zio::message_t{"hi",2}, zio::send_flags::dontwait);
        zio::debug("1 in main, leaving context");
    }
    zio::debug("in main, test 2");
    {
        zio::zactor_t actor(ctx, myactor, "hello world", false);
        zio::debug("2 in main, no sleep, send protocol actor");
        actor.link().send(zio::message_t{"hi",2}, zio::send_flags::dontwait);
        zio::debug("2 in main, leaving context");
    }
    zio::debug("in main, test 3");
    {
        zio::zactor_t actor(ctx, myactor, "fast exit", true);
        zio::debug("3 in main, sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        zio::debug("3 in main, send protocol message");
        actor.link().send(zio::message_t{"hi",2}, zio::send_flags::dontwait);
        zio::debug("3 in main, leaving context");
    }
    zio::debug("in main, test 4");
    {
        zio::zactor_t actor(ctx, myactor, "fast exit", true);
        zio::debug("4 in main, no sleep, send protocol message");
        actor.link().send(zio::message_t{"hi",2}, zio::send_flags::dontwait);
        zio::debug("4 in main, leaving context");
    }

    zio::debug("in main, exiting");

}
