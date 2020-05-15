#include "zio/zmq.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

std::string addr = "tcp://127.0.0.1:5678";
//const int ntotal = 100000;
const int ntotal = 2000;

void reqer()
{
    zmq::context_t ctx;
    int x = 0;
    while (true) {
        zmq::socket_t s(ctx, zmq::socket_type::req);
        s.connect(addr);
        s.set(zmq::sockopt::rcvtimeo, 100);
        s.set(zmq::sockopt::linger, 0);
        s.set(zmq::sockopt::immediate, 1);

        zmq::message_t msg{&x, sizeof(int)};
        s.send(msg, zmq::send_flags::none);
        auto res = s.recv(msg, zmq::recv_flags::none);
        if (res) {
            int newx = *msg.data<int>();
            std::cout << "c:" << x << std::endl;
            assert(newx - x == 1);
            x = newx;
        }
        else {
            std::cout << "c:" << std::endl;
        }
        s.disconnect(addr);
        if (x > ntotal) { break; }
    }
}

void reper()
{
    using namespace std::chrono_literals;
    zmq::context_t ctx;
    zmq::socket_t s(ctx, zmq::socket_type::rep);
    s.bind(addr);
    int x=0;
    while (true) {
        {
            zmq::message_t msg{&x, sizeof(int)};
            auto res = s.recv(msg, zmq::recv_flags::none);
            res.reset();
            x = *msg.data<int>();
        }
        ++x;
        //std::this_thread::sleep_for(98ms);
        {
            zmq::message_t msg{&x, sizeof(int)};
            s.send(msg, zmq::send_flags::none);
        }
        if (x > ntotal) { break; }
    }
}

int main()
{
    std::thread req(reqer);
    std::thread rep(reper);
    rep.join();
    req.join();
    return 0; 
}

