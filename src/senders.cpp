#include "zio/senders.hpp"

zio::TextSender::TextSender(portptr_t portptr, origin_t origin)
    : port(portptr)
    , msg({{0,"TEXT",""},{origin,0,0}})
{
}

void zio::TextSender::operator()(zio::level::MessageLevel lvl,
                                 const std::string& log)
{
    msg.set_level(lvl);
    msg.next(payload_t(log.begin(), log.end()));
    port->socket().send(msg.encode());
}


zio::JsonSender::JsonSender(portptr_t portptr, origin_t origin)
    : port(portptr)
    , msg({{0,"JSON",""},{origin,0,0}})
{
}

void zio::JsonSender::operator()(zio::level::MessageLevel lvl, 
                                 const zio::json& met)
{           
    msg.set_level(lvl);
    std::string s = met.dump();
    msg.next(payload_t(s.begin(), s.end()));
    port->socket().send(msg.encode());
}

