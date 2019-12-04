// Test producer/consumer layers

#include "zio/producer.hpp"
//#include "zio/consumer.hpp"

void a_copy(zio::Producer<zio::JSON> copy)
{
    copy.info({{"foo","bar"}, {"baz","quax"}});
}

int main()
{
    zio::Producer<zio::TEXT> log(0xdeadbeaf);
    log.socket()->bind("inproc://prodcons");
    log.info("here be some info logging");
    log(zio::level::warning, "oh no!");

    // // cross construct
    zio::Producer<zio::JSON> met(log);
    met.info({{"answer",42}});

    // check copy
    a_copy(met);

    zio::Producer<zio::BUFF> bin(log);
    std::string blah = "this is pretend binary";
    bin.info(zio::BUFF(blah.size(), blah.data()));
    bin.warning(blah);
    bin.warning("this is some what of a type abuse");

//    zio::Consumer cons(ZMQ_SUB);

    return 0;
}
