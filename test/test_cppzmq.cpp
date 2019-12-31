#include "zio/zio.hpp"

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
#include <iostream>
using namespace std;

int main()
{

    zio::context_t ctx;
    zio::socket_t s(ctx, ZMQ_SERVER);
    s.bind("inproc://testcppzmq");
    zio::socket_t c(ctx, ZMQ_CLIENT);
    c.connect("inproc://testcppzmq");

    usleep(1000);

    const std::string hw = "Hello World!";

    cerr << "sending " << hw << endl;
    auto ses = c.send(zio::buffer(hw.data(), hw.size()));
    assert (ses);
    assert (*ses == hw.size());
    assert (ses.value() == hw.size());


    cerr << "receiving\n";
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
