/*! Test ZIO domo

  Default test

  $ ./build/test_gdp

  Span socket types and client/worker numbers

  $ ./build/test_gdp router 1 1
  $ ./build/test_gdp server 1 1
  $ ./build/test_gdp router 10 1
  $ ./build/test_gdp server 1 10
  $ ./build/test_gdp router 10 10
  $ ./build/test_gdp server 10 10

  Cross language test using Python workers (run each in own terminal)

  $ ./build/test_gdp router 1 0
  $ ziodomo echo --verbose -s dealer -a tcp://127.0.0.1:5555

  $ ./build/test_gdp server 1 0
  $ ziodomo echo --verbose -s client -a tcp://127.0.0.1:5555

 */

#include "zio/domo/broker.hpp"
#include "zio/domo/client.hpp"
#include "zio/domo/worker.hpp"
#include "zio/logging.hpp"
#include "zio/actor.hpp"

using namespace zio::domo;

void countdown_echo(zio::socket_t& link, std::string address, int socktype)
{
    zio::context_t ctx;
    zio::socket_t sock(ctx, socktype);
    Client client(sock, address);

    link.send(zio::message_t{}, zio::send_flags::none);

    int countdown = 4;

    while (countdown) {
        --countdown;
        std::stringstream ss;
        if (countdown) {
            ss << countdown << "...";
        }
        else {
            ss << "blast off!";
        }
        zio::multipart_t mmsg(ss.str());
        client.send("echo", mmsg);
        mmsg.clear();
        client.recv(mmsg);
        if (mmsg.empty()) {
            zio::error("countdown echo timeout");
            break;
        }
        {
            std::stringstream ss;
            ss << "countdown echo [" << mmsg.size() << "]:";
            while (mmsg.size()) {
                ss << "\n\t" << mmsg.popstr();
            }
            zio::info(ss.str());
        }
    }
    link.send(zio::message_t{}, zio::send_flags::none);
    zio::message_t die;
    auto res = link.recv(die, zio::recv_flags::none);
    res = {};                   // don't care
    zio::debug("countdown echo exiting");
}


void doit(int serverish, int clientish, int nclients, int nworkers)
{
    std::stringstream ss;
    ss<<"main doit("<<serverish<<","<<clientish<<","<<nworkers<<","<<nclients<<")";
    zio::info(ss.str());

    zio::context_t ctx;
    std::string broker_address = "tcp://127.0.0.1:5555";

    std::vector<zio::zactor_t*> clients;
    std::vector<std::pair<std::string, zio::zactor_t*> > actors;


    zio::debug("main make broker actor");
    actors.push_back({"broker",new zio::zactor_t(ctx, broker_actor, broker_address, serverish)});

    while (nworkers--) {
        zio::debug("main make worker actor");
        actors.push_back({"worker",new zio::zactor_t(ctx, echo_worker, broker_address, clientish)});
    }

    while (nclients--) {
        zio::debug("main make client actor");
        auto client = new zio::zactor_t(ctx, countdown_echo, broker_address, clientish);
        actors.push_back({"client",client});
        clients.push_back(client);
    }

    for (auto client : clients) {
        zio::debug("main wait for client");
        zio::message_t done;
        auto res = client->link().recv(done, zio::recv_flags::none);
        res = {};                   // don't care
    }

    // terminate backwards
    for (auto it = actors.rbegin(); it != actors.rend(); ++it) {
        zio::debug("main terminate actor " + it->first);
        zio::zactor_t* actor = it->second;
        actor->link().send(zio::message_t{}, zio::send_flags::none);
        delete actor;
    }
    zio::debug("main doit exiting");
}

int main(int argc, char* argv[])
{
    zio::catch_signals();

    std::string which = "server";
    if (argc > 1) {
        which = argv[1];
    }

    int nclients = 1;
    if (argc > 2) {
        nclients = atoi(argv[2]);
    }
    int nworkers = 1;
    if (argc > 3) {
        nworkers = atoi(argv[3]);
    }

    if (which == "server") {
        doit(ZMQ_SERVER, ZMQ_CLIENT, nclients, nworkers);
    }
    else {
        doit(ZMQ_ROUTER, ZMQ_DEALER, nclients, nworkers);
    }
    return 0;
}


