// https://github.com/zeromq/libzmq/issues/3048

#include <string>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdio>

#include <zmq.h>

static void *ctx;
static const char *endpoint = "inproc://tmp";
// static const char *endpoint = "tcp://127.0.0.1:8899";

void client(std::string str)
{
    auto socket = zmq_socket(ctx, ZMQ_CLIENT);
    zmq_connect(socket, endpoint);

    for (;;) {
        // send msgs
        {
            zmq_msg_t msg;
            zmq_msg_init_size(&msg, str.size() + 1);
            memcpy(zmq_msg_data(&msg), str.c_str(), str.size() + 1);
            zmq_msg_send(&msg, socket, 0);
            zmq_msg_close(&msg);
        }

        // recv hearbeats
        {
            zmq_pollitem_t polls[1] = {
                {socket, 0, ZMQ_POLLIN, 0},
            };
            auto rc = zmq_poll(polls, 1, 10);
            if (rc > 0) {
                zmq_msg_t msg;
                zmq_msg_init(&msg);
                zmq_msg_recv(&msg, socket, 0);
                printf("client recv msg: %s\n", (char *)zmq_msg_data(&msg));
            }
        }
    }
}

void server()
{
    using namespace std::chrono;

    std::unordered_set<uint32_t> routings;
    auto heartbeat_timestamp = steady_clock::now();
    char heartbeat_str[] = "heartbeat from server";
    auto socket = zmq_socket(ctx, ZMQ_SERVER);
    zmq_bind(socket, endpoint);

    for (;;) {
        // recv msgs
        {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            zmq_msg_recv(&msg, socket, 0);
            routings.insert(zmq_msg_routing_id(&msg));
            printf("server recv msg: %s\n", (char *)zmq_msg_data(&msg));
            zmq_msg_close(&msg);
        }

        // refresh heartbeats
        auto now = steady_clock::now();
        auto delta =
            duration_cast<milliseconds>(now - heartbeat_timestamp).count();
        if (delta > 500) {
            heartbeat_timestamp = now;
            zmq_msg_t msg;
            zmq_msg_init_size(&msg, sizeof(heartbeat_str));
            memcpy(zmq_msg_data(&msg), heartbeat_str, sizeof(heartbeat_str));
            for (auto it : routings) {
                zmq_msg_t copy_msg;
                zmq_msg_init(&copy_msg);
                zmq_msg_copy(&copy_msg, &msg);
                zmq_msg_set_routing_id(&copy_msg, it);
                zmq_msg_send(&copy_msg, socket, 0);
                zmq_msg_close(&copy_msg);
            }
            zmq_msg_close(&msg);
        }
    }
}

int main()
{
    ctx = zmq_ctx_new();
    zmq_ctx_set(ctx, ZMQ_IO_THREADS, 4);
    std::thread t1(client, "hello from thread 1");
    std::thread t2(client, "hello from thread 2");
    std::thread t3(server);
    int countdown = 2;
    while (countdown--) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    return 0;
}
