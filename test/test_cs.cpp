#include <czmq.h>

#include <map>
#include <vector>

void test_owoa(bool backwards)
{
    const int nclients = 3;
    std::vector<zsock_t*> clients;
    for (int ind=0; ind<nclients; ++ind) {
        zsock_t* c = zsock_new(ZMQ_CLIENT);
        //zsock_t* c = zsock_new(ZMQ_PUSH);
        assert(c);
        clients.push_back(c);
    }
    zsock_t* s = zsock_new(ZMQ_SERVER);
    //zsock_t* s = zsock_new(ZMQ_PULL);
    assert(s);

    const char* mode[] = {"forward","backward"};
    zsys_debug("made server and %d clients in %s mode",
               nclients, mode[backwards]);

    const int base_port = 5670;
    // const char * addr = "tcp://127.0.0.1:%d";
    const char * addr = "ipc://test_cs%d.ipc";
    // const char * addr = "inproc://test_cs%d";
    if (backwards) {            // bind clients, connect server
        for (int ind=0; ind<nclients; ++ind) {
            auto c = clients[ind];
            const int port = base_port + ind;
            const int p = zsock_bind(c, addr, port);
            if (p<0) {
                zsys_error("backwards failed bind %s port %d",
                           addr, port);
            }
            assert (p >= 0);
            int rc = zsock_connect(s, addr, port);
            assert (rc >= 0);
        }
    }
    else {                      // bind server, connect clients
        const int port = base_port;
        const int p = zsock_bind(s, addr, port);
        if (p<0) {
            zsys_error("forwards failed bind %s port %d",
                       addr, port);
        }
        assert(p >= 0);
        for (auto c : clients) {
            int rc = zsock_connect(c, addr, port);
            assert (rc >= 0);
        }
    }
        
    zsys_debug("bind and connect done");
    //zclock_sleep(1000);

    const int basen = 42;
    // client send
    for (int ind=0; ind<nclients; ++ind) {
        auto c = clients[ind];
        const int n = basen + ind;
        zsys_debug("client sending %d", n);
        const int rc = zsock_bsend(c,"4",n);
        assert(rc >= 0);
    }

    zsys_debug("clients sent");

    std::map<uint32_t, int> rid2n;

    // server recv
    for (int ind=0; ind<nclients; ++ind) {
        int n = 0;
        const int rc = zsock_brecv(s,"4",&n);
        uint32_t rid = zsock_routing_id(s);
        zsys_debug("server recving %d from 0x%x", n, rid);
        rid2n[rid] = n;
    }
    
    zsys_debug("server received");

    // server send
    for (auto& rn : rid2n) {
        zsock_set_routing_id(s, rn.first);
        zsys_debug("server sending %d to 0x%x", rn.second, rn.first);
        int rc = zsock_bsend(s,"4",rn.second);
        assert (rc >= 0);
    }

    zsys_debug("server sent");

    // client recv
    for (int ind=0; ind<nclients; ++ind) {
        int n=0;
        int rc = zsock_brecv(clients[ind],"4",&n);
        zsys_debug("client recving %d", n);
        assert(n == basen + ind);
    }

    zsys_debug ("clients received");

    for (auto c : clients) {
        zsock_destroy(&c);
    }
    zsock_destroy(&s);
}

int main()
{
    test_owoa(false);
    test_owoa(true);

    return 0;
}
