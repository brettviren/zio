#include "zio/senders.hpp"

zio::TextSender::TextSender(zio::portptr_t port)
    : port(port)
    , msg({{level::undefined,"TEXT",""},{0,0,0}})
{
}

void zio::TextSender::operator()(zio::level::MessageLevel lvl,
                                 const std::string& log)
{
    msg.set_level(lvl);
    msg.payload().clear();
    msg.payload().push_back(payload_t(log.begin(), log.end()));
    port->send(msg);
}


zio::JsonSender::JsonSender(zio::portptr_t port)
    : port(port)
    , msg({{level::undefined,"JSON",""},{0,0,0}})
{
}

void zio::JsonSender::operator()(zio::level::MessageLevel lvl, 
                                 const zio::json& met)
{           
    msg.set_level(lvl);
    msg.payload().clear();
    std::string s = met.dump();
    msg.payload().push_back(payload_t(s.begin(), s.end()));
    port->send(msg);
}
