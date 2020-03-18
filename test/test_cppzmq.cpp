#include "zio/cppzmq.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"

#ifndef ZMQ_CPP11
#error c++11 should be defined
#endif

#ifndef ZMQ_CPP14
#error c++14 should be defined
#endif

#ifndef ZMQ_CPP17
#error c++17 should be defined
#endif

#include <unistd.h>
using namespace std;

int main()
{
    zio::init_all();

    zio::context_t ctx;
    zio::socket_t s(ctx, ZMQ_SERVER);
    s.bind("inproc://testcppzmq");
    zio::socket_t c(ctx, ZMQ_CLIENT);
    c.connect("inproc://testcppzmq");

    usleep(1000);

    const std::string hw = "Hello World!";

    zio::debug("sending {}", hw);
    zio::message_t smsg(hw.data(), hw.size());
    auto ses = c.send(smsg, zio::send_flags::none);
    assert (ses);
    assert (*ses == hw.size());
    assert (ses.value() == hw.size());


    zio::debug("receiving");
    zio::message_t rmsg;
    auto res = s.recv(rmsg);
    assert (res);
    assert (*res == hw.size());
    assert (res.value() == hw.size());
    assert (rmsg.size() == hw.size());
    std::string hw2(static_cast<char*>(rmsg.data()), rmsg.size());
    assert (hw2.size() == hw.size());
    assert (hw2 == hw);

    assert (rmsg.routing_id());

    ses = s.send(rmsg, zio::send_flags::none);
    assert(ses);

    res = c.recv(rmsg);
    assert (res);
    assert (*res == hw.size());
    assert (res.value() == hw.size());
    assert (rmsg.size() == hw.size());
    std::string hw3(static_cast<char*>(rmsg.data()), rmsg.size());
    assert (hw3.size() == hw.size());
    assert (hw3 == hw);
    

    return 0;
}
