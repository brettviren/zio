#include "zio/cppzmq.hpp"
#include "zio/logging.hpp"
#include "zio/main.hpp"
#include <unistd.h>
#include <chrono>
#include <thread>

#include <map>

const int server_type = ZMQ_SERVER;
const int client_type = ZMQ_CLIENT;

/// inproc hangs.  no messages ever get received by server. tcp/ipc okay.
//const char* addr = "inproc://test_tcs";
//const char* addr = "tcp://127.0.0.1:5678";
const char* addr = "ipc://test_tcs.ipc";

typedef std::chrono::duration<int64_t,std::micro> microseconds_type;

// a very very ugly server
static
void server(zio::socket_t& s)
{
    std::map<uint32_t,uint32_t> rids;
    std::map<uint32_t, std::vector<int> > tosend;

    zio::poller_t<> poller;
    poller.add(s, zio::event_flags::pollin);
    zio::info("server: loop starts");
    const auto wait = std::chrono::milliseconds{2000};
    
    int64_t ttot=0, tmin=0, tmax=0;
    int count = 0;

    int dead = 0;

    while (true) {
        zio::info("server: polling {}", count);
        auto t0 = std::chrono::high_resolution_clock::now();
        std::vector<zio::poller_event<>> events(1);
        try {
            int rc = poller.wait_all(events, wait);
            if (rc == 0) {
                zio::info("server: poll times out");
                break;
            }
        } catch (zio::error_t e) {
            zio::info("server: poller exception: {}", e.what());
            return;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        const microseconds_type dtus = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
        const int64_t dt = dtus.count();
        
        if (count == 0) {
            tmin = tmax = dt;
        }
        else {
            tmin = std::min(tmin, dt);
            tmax = std::max(tmax, dt);
        }
        count += 1;
        ttot += dt;
        zio::info("server: #{} [{}, {}] <{}> tot={} dt={} [us]", count,
                tmin, tmax, (ttot/count), ttot, dt);
            

        zio::message_t msg;
        auto res = s.recv(msg);
        assert(res);
        uint32_t rid = msg.routing_id();
        assert (rid > 0);

        if (msg.size() == 0) {
            if (dead) {
                break;          // bail after more than 1
            }
            dead += 1;
        }

        // the "message"
        int them = *static_cast<int*>(msg.data());

        zio::debug("server: recvd {} {}", rid, them);

        if (rids.empty()) {
            rids[rid]=0;
            tosend[rid].push_back(them);
            continue;
        }
        if (rids.size() == 1) {
            uint32_t orid = rids.begin()->first;
            if (rid == orid) {  // more of the same
                tosend[rid].push_back(them);
                continue;
            }
            // now have 2
            rids[rid] = orid;
            rids[orid] = rid;
        }
        tosend[rid].push_back(them);

        for (const auto& rid_v : tosend) {
            uint32_t rid = rid_v.first;
            for (auto them : rid_v.second) {
                uint32_t orid = rids[rid];
                zio::message_t msg(&them, sizeof(int));        
                msg.set_routing_id(orid);
                auto ses = s.send(msg, zio::send_flags::none);
                assert(ses);
            }
        }
        tosend.clear();
    }
}

static
void client(zio::socket_t& c, int me)
{
    zio::debug("client {}: starts", me);

    zio::poller_t<> poller;
    poller.add(c, zio::event_flags::pollin);
    const auto wait = std::chrono::milliseconds{2000};

    int zzz = 1000000;
    zio::info("client {}: sleeps for {}", me, zzz);
    usleep(zzz);
    for (int count=0; count<2000; ++count) {
        zio::message_t msg(&me, sizeof(int));        
        zio::debug("client {}: send", me);
        c.send(msg, zio::send_flags::none);

        zio::debug("client {}: polling", me);
        std::vector<zio::poller_event<>> events(1);
        try {
            int rc = poller.wait_all(events, wait);
            if (rc == 0) {
                zio::info("client: poll times out");
                return;
            }
        } catch (zio::error_t e) {
            zio::info("client: poller exception: {}", e.what());
            return;
        }

        zio::debug("client {}: recv", me);
        auto res = c.recv(msg);
        assert(res);
        int them = *static_cast<int*>(msg.data());
        zio::debug("client {}: got {}", me, them);
    }
    zio::debug("client {}: send final", me);
    c.send(zio::message_t(), zio::send_flags::none);
}
int main()
{
    zio::init_all();
    zio::info("test_tcs starting");

    zio::context_t ctx;
    zio::socket_t s(ctx, server_type);
    s.bind(addr);

    std::thread ser(server, std::ref(s));
    zio::info("sleeping between server and client thread starts");
    usleep(100000);


    zio::socket_t c1(ctx, client_type);
    c1.connect(addr);
    zio::debug("client 1: connected");

    zio::socket_t c2(ctx, client_type);
    c2.connect(addr);
    zio::debug("client 2: connected");

    std::thread cli1(client, std::ref(c1), 1);
    std::thread cli2(client, std::ref(c2), 2);    

    cli1.join();
    cli2.join();
    ser.join();

}
