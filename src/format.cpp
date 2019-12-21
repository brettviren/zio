#include "zio/format.hpp"

std::string zio::converter::text_t::operator()(const payload_t& buf)
{
    return std::string(buf.begin(), buf.end());
}
zio::payload_t zio::converter::text_t::operator()(const std::string& str)
{
    return payload_t(str.begin(), str.end());
}

zio::json zio::converter::json_t::operator()(const payload_t& buf)
{
    std::string s(buf.begin(), buf.end());
    return json::parse(s);
}
zio::payload_t zio::converter::json_t::operator()(const json& jobj)
{
    std::string s = jobj.dump();
    return payload_t(s.begin(), s.end());
}

zio::json zio::converter::bson_t::operator()(const payload_t& buf)
{
    return json::from_bson(buf);
}
zio::payload_t zio::converter::bson_t::operator()(const json& jobj)
{
    return json::to_bson(jobj);
}


zio::json zio::converter::cbor_t::operator()(const payload_t& buf)
{
    return json::from_cbor(buf);
}
zio::payload_t zio::converter::cbor_t::operator()(const json& jobj)
{
    return json::to_cbor(jobj);
}

zio::json zio::converter::msgp_t::operator()(const payload_t& buf)
{
    return json::from_msgpack(buf);
}
zio::payload_t zio::converter::msgp_t::operator()(const json& jobj)
{
    return json::to_msgpack(jobj);
}

zio::json zio::converter::ubjs_t::operator()(const payload_t& buf)
{
    return json::from_ubjson(buf);
}
zio::payload_t zio::converter::ubjs_t::operator()(const json& jobj)
{
    return json::to_ubjson(jobj);
}

// void zio::send(Socket& sock,
//                level::MessageLevel lvl, const std::string& format,
//                const payload_t& buf, const std::string& label)
// {
//     zmsg_t* msg = zmsg_new();
//     zmsg_addstrf(msg, "ZIO%d%4s%s", lvl, format.c_str(), label.c_str());
//     const int ncoords = 3;
//     uint64_t coords[ncoords] = {m_ctx.origin, m_ctx.gf(), m_seqno++};
//     zmsg_addmem(msg, coords, ncoords*sizeof(uint64_t));
//     zmsg_addmem(msg, buf.data(), buf.size());
//     sock.send(&msg);
// }
