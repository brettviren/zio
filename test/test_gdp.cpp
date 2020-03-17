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
#include "zio/zmq_actor.hpp"

using namespace zio::domo;

void countdown_echo(zmq::socket_t& pipe, std::string address, int socktype)
{
    zio::console_log log;

    zmq::context_t ctx;
    zmq::socket_t sock(ctx, socktype);
    Client client(sock, address, log);

    pipe.send(zmq::message_t{}, zmq::send_flags::none);

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
        zmq::multipart_t mmsg(ss.str());
        client.send("echo", mmsg);
        mmsg.clear();
        client.recv(mmsg);
        if (mmsg.empty()) {
            log.error("countdown echo timeout");
            break;
        }
        {
            std::stringstream ss;
            ss << "countdown echo [" << mmsg.size() << "]:";
            while (mmsg.size()) {
                ss << "\n\t" << mmsg.popstr();
            }
            log.info(ss.str());
        }
    }
    pipe.send(zmq::message_t{}, zmq::send_flags::none);
    zmq::message_t die;
    auto res = pipe.recv(die);
    log.debug("countdown echo exiting");
}


void doit(int serverish, int clientish, int nclients, int nworkers)
{
    zio::console_log log;

    std::stringstream ss;
    ss<<"main doit("<<serverish<<","<<clientish<<","<<nworkers<<","<<nclients<<")";
    log.info(ss.str());

    zmq::context_t ctx;
    std::string broker_address = "tcp://127.0.0.1:5555";

    std::vector<zmq::actor_t*> clients;
    std::vector<std::pair<std::string, zmq::actor_t*> > actors;


    log.debug("main make broker actor");
    actors.push_back({"broker",new zmq::actor_t(ctx, broker_actor, broker_address, serverish)});

    while (nworkers--) {
        log.debug("main make worker actor");
        actors.push_back({"worker",new zmq::actor_t(ctx, echo_worker, broker_address, clientish)});
    }

    while (nclients--) {
        log.debug("main make client actor");
        auto client = new zmq::actor_t(ctx, countdown_echo, broker_address, clientish);
        actors.push_back({"client",client});
        clients.push_back(client);
    }

    for (auto client : clients) {
        log.debug("main wait for client");
        zmq::message_t done;
        auto res = client->pipe().recv(done);
    }

    // terminate backwards
    for (auto it = actors.rbegin(); it != actors.rend(); ++it) {
        log.debug("main terminate actor " + it->first);
        zmq::actor_t* actor = it->second;
        actor->pipe().send(zmq::message_t{}, zmq::send_flags::none);
        delete actor;
    }
    log.debug("main doit exiting");
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


