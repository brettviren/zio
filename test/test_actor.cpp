/*! cppzmq does not directly provide an actor pattern.
 * this tests a simple one. */


#include "zio/actor.hpp"

#include <thread>
#include <sstream>
#include <iostream>


#include <chrono>


static
void myactor(zio::socket_t& link, std::string greeting, bool fast_exit)
{
    std::cerr << greeting << std::endl;
    std::cerr << "myactor says hi" << std::endl;

    // ready
    link.send(zio::message_t{}, zio::send_flags::none);

    zio::message_t msg;
    std::cerr << "myactor: wait for app protocol message" << std::endl;
    auto res1 = link.recv(msg);
    assert(res1);
    std::cerr << "myactor: got protocol message size " << msg.size() << std::endl;
    assert(msg.size() == 2);
    assert(msg.to_string() == "hi");

    if (fast_exit) {
        std::cerr << "myactor: exit early\n";
        return;
    }

    std::cerr << "myactor: simulating 1 second of work, try to Ctrl-c me\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));


    std::cerr << "myactor: waiting for termination message\n";
    zio::message_t rmsg;
    auto res2 = link.recv(rmsg);
    assert(res2);
    assert(rmsg.to_string() == "$TERM");
    std::cerr << "myactor: exiting\n";
}


int main()
{
    zio::context_t ctx;

    std::cerr << "in main, test 1\n";
    {
        zio::zactor_t actor(ctx, myactor, "hello world", false);
        std::cerr << "1 in main, sleep\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        std::cerr << "1 in main, send protocol message\n";
        actor.link().send(zio::message_t{"hi",2}, zio::send_flags::dontwait);
        std::cerr << "1 in main, leaving context\n";
    }
    std::cerr << "in main, test 2\n";
    {
        zio::zactor_t actor(ctx, myactor, "hello world", false);
        std::cerr << "2 in main, no sleep, send protocol actor\n";
        actor.link().send(zio::message_t{"hi",2}, zio::send_flags::dontwait);
        std::cerr << "2 in main, leaving context\n";
    }
    std::cerr << "in main, test 3\n";
    {
        zio::zactor_t actor(ctx, myactor, "fast exit", true);
        std::cerr << "3 in main, sleep\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        std::cerr << "3 in main, send protocol message\n";
        actor.link().send(zio::message_t{"hi",2}, zio::send_flags::dontwait);
        std::cerr << "3 in main, leaving context\n";
    }
    std::cerr << "in main, test 4\n";
    {
        zio::zactor_t actor(ctx, myactor, "fast exit", true);
        std::cerr << "4 in main, no sleep, send protocol message\n";
        actor.link().send(zio::message_t{"hi",2}, zio::send_flags::dontwait);
        std::cerr << "4 in main, leaving context\n";
    }

    std::cerr << "in main, exiting\n";

}
