// Test producer/consumer layers

#include "zio/producer.hpp"
//#include "zio/consumer.hpp"

template<typename PROD>
void a_copy(PROD copy)
{
    copy.info({{"foo","bar"}, {"baz","quax"}});
}

int main()
{
    zio::Producer<zio::TEXT> log(0xdeadbeaf, ZMQ_PUB);
    log.socket()->bind("inproc://prodcons");
    log.info("here be some info logging");
    log(zio::level::warning, "oh no!");

    // cross construct
    zio::Producer<zio::JSON> met(log);
    met.info({{"answer",42}});

    // check copy
    a_copy(met);

    // cross-type send
    met.send(zio::level::debug, zio::TEXT{"foo"});

    // std::string buf = "this is binary";
    // prod.info(zio::BUFF{buf.size(), buf.data()}); // should be BUFF

//    zio::Consumer cons(ZMQ_SUB);

    return 0;
}
