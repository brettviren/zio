/*! cppzmq does not directly provide an actor pattern.
 * this tests a simple one. */

#include "zmq.hpp"
#include "zmq_actor.hpp"


#include <thread>
#include <sstream>
#include <iostream>


#include <chrono>


static
void myactor(zmq::socket_t& pipe, std::string greeting, bool fast_exit)
{
    std::cerr << greeting << std::endl;
    std::cerr << "myactor says hi" << std::endl;

    pipe.send(zmq::message_t{}, zmq::send_flags::none);

    zmq::poller_t<> poller;
    poller.add(pipe, zmq::event_flags::pollin);
    std::vector< zmq::poller_event<> > events(1);
    const std::chrono::milliseconds timeout{500};
    int rc = poller.wait_all(events, timeout);
    if (rc) {
        std::cerr << "there is stuff in the pipe " << rc << std::endl;
        zmq::message_t msg;
        auto res = pipe.recv(msg);
        assert(res);
        std::cerr << "pipe has message of size " << msg.size() << std::endl;
    }
    /// we get this if our owner kills the actor 
    // assert(rc == 0);            // should be nothing in this pipe


    if (fast_exit) {
        std::cerr << "myactor exit early\n";
        return;
    }

    std::cerr << "myactor simulating work, try to Ctrl-c me\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));


    std::cerr << "myactor waiting for death\n";

    zmq::message_t rmsg;
    auto res = pipe.recv(rmsg);
}



// int old_main()
// {
//     zmq::context_t ctx;
//     auto [apipe,mypipe] = zmq::create_pipe(ctx);
//     std::thread actor(myactor, std::move(apipe), "hello world");

//     std::cerr << "in main, receiving ready\n";

//     zmq::message_t rmsg;
//     auto res = mypipe.recv(rmsg);

//     std::cerr << "in main, terminating actor\n";
//     mypipe.send(zmq::message_t{}, zmq::send_flags::none);

//     std::cerr << "in main, joining\n";
//     actor.join();

//     std::cerr << "in main, exiting\n";
// }

int main()
{
    zmq::context_t ctx;

    {
        zmq::actor_t actor(ctx, myactor, "hello world", false);
        std::cerr << "in main, sleep\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        std::cerr << "in main, terminating actor\n";
        actor.pipe().send(zmq::message_t{}, zmq::send_flags::dontwait);
        std::cerr << "in main, leaving context\n";
    }
    {
        zmq::actor_t actor(ctx, myactor, "hello world", false);
        std::cerr << "in main, no sleep, terminating actor\n";
        actor.pipe().send(zmq::message_t{}, zmq::send_flags::dontwait);
        std::cerr << "in main, leaving context\n";
    }
    {
        zmq::actor_t actor(ctx, myactor, "fast exit", true);
        std::cerr << "in main, sleep\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        std::cerr << "in main, terminating actor\n";
        actor.pipe().send(zmq::message_t{}, zmq::send_flags::dontwait);
        std::cerr << "in main, leaving context\n";
    }
    {
        zmq::actor_t actor(ctx, myactor, "hello world", true);
        std::cerr << "in main, no sleep, terminating actor\n";
        actor.pipe().send(zmq::message_t{}, zmq::send_flags::dontwait);
        std::cerr << "in main, leaving context\n";
    }

    std::cerr << "in main, exiting\n";

}
