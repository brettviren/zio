#include "zio/format.hpp"

std::string zio::converter::text_t::operator()(const byte_array_t& buf)
{
    return std::string(buf.begin(), buf.end());
}
zio::byte_array_t zio::converter::text_t::operator()(const std::string& str)
{
    return byte_array_t(str.begin(), str.end());
}

zio::json zio::converter::json_t::operator()(const byte_array_t& buf)
{
    std::string s(buf.begin(), buf.end());
    return json::parse(s);
}
zio::byte_array_t zio::converter::json_t::operator()(const json& jobj)
{
    std::string s = jobj.dump();
    return byte_array_t(s.begin(), s.end());
}

zio::json zio::converter::bson_t::operator()(const byte_array_t& buf)
{
    return json::from_bson(buf);
}
zio::byte_array_t zio::converter::bson_t::operator()(const json& jobj)
{
    return json::to_bson(jobj);
}


zio::json zio::converter::cbor_t::operator()(const byte_array_t& buf)
{
    return json::from_cbor(buf);
}
zio::byte_array_t zio::converter::cbor_t::operator()(const json& jobj)
{
    return json::to_cbor(jobj);
}

zio::json zio::converter::msgp_t::operator()(const byte_array_t& buf)
{
    return json::from_msgpack(buf);
}
zio::byte_array_t zio::converter::msgp_t::operator()(const json& jobj)
{
    return json::to_msgpack(jobj);
}

zio::json zio::converter::ubjs_t::operator()(const byte_array_t& buf)
{
    return json::from_ubjson(buf);
}
zio::byte_array_t zio::converter::ubjs_t::operator()(const json& jobj)
{
    return json::to_ubjson(jobj);
}

