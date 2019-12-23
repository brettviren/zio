#include "zio/senders.hpp"

zio::TextSender::TextSender(Node& node, std::string portname, int socket_type)
    : port(node.port(portname, socket_type))
    , msg({{level::undefined,"TEXT",""},{node.origin(),0,0}})
{
}

void zio::TextSender::operator()(zio::level::MessageLevel lvl,
                                 const std::string& log)
{
    msg.set_level(lvl);
    msg.next(payload_t(log.begin(), log.end()));
    auto data = msg.encode();
    zsys_debug("LOG[%d]: %s (len:%ld)", lvl, log.c_str(), data.size());
    port->socket().send(data);
}


zio::JsonSender::JsonSender(Node& node, std::string portname, int socket_type)
    : port(node.port(portname, socket_type))
    , msg({{level::undefined,"JSON",""},{node.origin(),0,0}})
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
