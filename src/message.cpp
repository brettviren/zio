#include "zio/message.hpp"

#include <sstream>

std::string zio::PrefixHeader::dumps() const
{
    std::stringstream ss;
    ss << "ZIO" << '0'+level << format << label;
    return s.str();
}

zio::Message::Message()
{
}
zio::Message::Message(const raw_msg_t& raw)
{
    this->parse(raw);
}
zio::Message::Message(const header_t h)
    : m_header(h)
{
}

void zio::Message::clear()
{
    m_header = header_t();
    m_payload.clear();
}
        
zio::Message::encoded_ zio::Message::encode() const
{
    zmsg_t* msg = zmsg_new();
    auto pre = m_header.prefix.dumps();
    zmsg_addstrf(msg, pre.c_str());
    zmsg_addmem(msg, &m_header.coord, sizeof(CoordHeader));
    for (auto& pl : m_payload) {
        zmsg_addmem(msg, pl.data(), pl.size());
    }
    zframe_t* frame = zmsg_encode(msg);
    encoded_t ret(frame->data(), frame->data()+frame->size());
    zframe_destroy(&frame);
    zmsg_destroy(&msg);
    // sigh, so much copying....
    // if this is a problem, we can probably directly pack into ZMQ format
    return ret;
}

void zio::Message::decode(const encoded_t& data)
{
    clear();

    zframe_t* frame = zframe_new(data.data(), data.size());
    zmsg_t* msg = zmsg_decode(frame);
    zframe_destroy(&frame);

    // prefix header
    char* h1 = zmsg_popstr(msg);
    if (!h1 or strlen(h1) < 8) {
        zmsg_destroy(&msg);
        free(h1);
        throw zio::message_error(1, "bad prefix header");
    }
    m_header.prefix.level = h1[3] - '0';
    std::string h1s = h1;
    m_header.prefix.format = h1s.substr(4,4);
    m_header.prefix.label = h1s.substr(8);
    free (h1);    
    
    // coord header
    frame = zmsg_pop(msg);
    if (!frame or zframe_size(frame) != 3*sizof(uint64_t)) {
        zframe_destroy(&frame);
        zmsg_destroy(&msg);
        throw zio::message_error(2, "bad coord header");
    }
    uint64_t* ogs = (uint64_t*)zframe_data(frame);
    header.origin = ogs[0];
    header.granule = ogs[1];
    header.seqno = ogs[2];
    zframe_destroy(&frame);    

    // payload
    while (zmsg_size(msg)) {
        frame = zmsg_pop(msg);
        if (!frame) {
            zframe_destroy(&frame);
            zmsg_destroy(&msg);
            throw zio::message_error(3,"bad payload");
        }
        std::uint8_t* b = zframe_data(frame);
        size_t s = zframe_size(frame);
        m_payload.emplace_back(b, b+s);
        zframe_destroy(&frame);
    }
}

const zio::header_t& zio::Message::header() const
{
    return m_header;
}

const zio::multiload_t& zio::Message::payload() const
{
    return m_payload;
}


