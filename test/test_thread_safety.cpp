// Try to place bounds on socket thread safety.
//
// In general the "classic" socket types are not thread safe (the
// context is).  We might wish to at least construct and
// bind()/connect() sockets in one thread and then hand them over to
// another thread for send()/recv() type use and final destruction.
//
// Wishes are like summer fireflies.

// If you build and run this, expect random hangs.  

#include "zio/zmq.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
using namespace std::chrono;

const int maxrun_ms = 1000;

// use same context for client and server sockets
#define SAME_CONTEXT 1

// spews message for each message
#define LOUD 0

void server(zmq::socket_t sock)
{
    int count=0;
    auto start = steady_clock::now();

    std::cerr << "server starts " << std::endl;
    while (true) {
        auto now = steady_clock::now();
        auto delta = duration_cast<milliseconds>(now - start).count();
        if (delta > maxrun_ms) {
            std::cerr << "server run time reached after " << delta << std::endl;
            break;
        }

        zmq::message_t msg(&count, sizeof(int));
        auto res = sock.send(msg, zmq::send_flags::none);
        if (!res) {
            std::cerr << "server send failed" << std::endl;
            break;
        }
#if LOUD
        std::cerr << "send: " << count << " after " << delta << std::endl;
#endif
        ++count;
    }
    std::cerr << "server exits with: " << count << std::endl;
}

void client(zmq::socket_t sock)
{
    zmq::poller_t<> poller;
    poller.add(sock, zmq::event_flags::pollin);
    const auto wait = std::chrono::milliseconds{100};

    int count = 0;
    auto start = steady_clock::now();
    while (true) {
        auto now = steady_clock::now();
        auto delta = duration_cast<milliseconds>(now - start).count();
        if (delta > maxrun_ms) { 
            std::cerr << "client run time reached" << std::endl;
            break; 
        }

        std::vector<zmq::poller_event<>> events(1);
        int rc = poller.wait_all(events, wait);
        if (!rc) {
            std::cerr << "client poll timeout" << std::endl;
            continue;
        }

        zmq::message_t msg;
        auto res = sock.recv(msg);
        if (!res) {
            std::cerr << "client recv failed" << std::endl;
            break;
        }
        if (msg.size() != sizeof(int)) { break; }
        count = *static_cast<int*>(msg.data());
#if LOUD
        std::cerr << "recv: " << count << std::endl;
#endif
    }
    std::cerr << "client exits with: " << count << std::endl;
}


void test_make_then_use(int sock_type, int tran_type)
{
    const std::vector<std::string> trans = {
        "inproc://test-make-then-use",
        "ipc://test-thread-safety.fifo",
        "tcp://127.0.0.1:8765", // possbile collision with other tests....
    };
    const std::vector<zmq::socket_type> sends = {
        zmq::socket_type::pair,
        zmq::socket_type::push,
        zmq::socket_type::pub,
    };
    const std::vector<zmq::socket_type> recvs = {
        zmq::socket_type::pair,
        zmq::socket_type::pull,
        zmq::socket_type::sub,
    };
        
    std::cerr << "socks: " << sock_type << " trans: " << tran_type << std::endl;

    std::cerr << "make server:" << std::endl;
    zmq::context_t sctx;
    std::thread ser;
    {
        zmq::socket_t ssock(sctx, sends[sock_type]);
        ssock.bind(trans[tran_type]);
        ser = std::thread(server, std::move(ssock));
    }
    
#if SAME_CONTEXT
#define cctx sctx
#else
    zmq::context_t cctx;
#endif
    std::cerr << "make client:" << std::endl;
    std::thread cli;
    {
        zmq::socket_t rsock(cctx, recvs[sock_type]);
        if (recvs[sock_type] == zmq::socket_type::sub) {
            rsock.set(zmq::sockopt::subscribe, "");
        }
        rsock.connect(trans[tran_type]);
        cli = std::thread(client, std::move(rsock));
    }    

    std::cerr << "join client:" << std::endl;
    cli.join();

#if SAME_CONTEXT
    std::cerr << "same context used" << std::endl;
#endif

    std::cerr << "join server:" << std::endl;
    ser.join();


    std::cerr << "done." << std::endl;
}


int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "usage: test_thread_safety <isocks> <itrans>" << std::endl;
        std::cerr << "this test tends to hang" << std::endl;
        return 0;
    }
    int isocks = atoi(argv[1]);
    int itrans = atoi(argv[2]);

    test_make_then_use(isocks, itrans);
    // for (int itrans = 0; itrans < 3; ++itrans) {
    //     for (int isocks = 0; isocks < 3; ++isocks) {
    //         test_make_then_use(isocks, itrans);
    //     }
    // }
    return 0;
}
