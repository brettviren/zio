#include "zio/message.hpp"
#include "zio/exceptions.hpp"
#include <czmq.h>
#include <sstream>
#include <chrono>

const char* zio::level::name(zio::level::MessageLevel lvl)
{
    const char* level_names[] = {
        "undefined",
        "trace","verbose","debug","info","summary",
        "warning","error","fatal",
        0
    };
    return level_names[lvl];
}



std::string zio::PrefixHeader::dumps() const
{
    std::stringstream ss;
    char num = '0' + level;
    ss << "ZIO" << num << format;
    if (label.size() > 0) {
        ss << label;
    }
    return ss.str();
}

zio::Message::Message()
{
}

zio::Message::Message(const encoded_t& data)
{
    this->decode(data);
}

zio::Message::Message(const header_t h, const multiload_t& pl)
    : m_header(h)
    , m_payload(pl.begin(), pl.end())
{
}

void zio::Message::clear()
{
    m_header = header_t();
    m_payload.clear();
}
        
zio::Message::encoded_t zio::Message::encode() const
{
    zmsg_t* msg = zmsg_new();
    auto pre = m_header.prefix.dumps();
    zmsg_addstrf(msg, pre.c_str());
    zmsg_addmem(msg, &m_header.coord, sizeof(CoordHeader));
    for (auto& pl : m_payload) {
        zmsg_addmem(msg, pl.data(), pl.size());
    }
    zframe_t* frame = zmsg_encode(msg);
    encoded_t ret(zframe_data(frame), zframe_data(frame)+zframe_size(frame));
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
        throw zio::message_error::create(1, "bad prefix header");
    }
    m_header.prefix.level = level::MessageLevel(h1[3] - '0');
    std::string h1s = h1;
    m_header.prefix.format = h1s.substr(4,4);
    if (h1s.size() > 8) {
        m_header.prefix.label = h1s.substr(8);
    }
    free (h1);    
    
    // coord header
    frame = zmsg_pop(msg);
    if (!frame or zframe_size(frame) != 3*sizeof(uint64_t)) {
        zframe_destroy(&frame);
        zmsg_destroy(&msg);
        throw zio::message_error::create(2, "bad coord header");
    }
    uint64_t* ogs = (uint64_t*)zframe_data(frame);
    m_header.coord.origin = ogs[0];
    m_header.coord.granule = ogs[1];
    m_header.coord.seqno = ogs[2];
    zframe_destroy(&frame);    

    // payload
    while (zmsg_size(msg)) {
        frame = zmsg_pop(msg);
        if (!frame) {
            zframe_destroy(&frame);
            zmsg_destroy(&msg);
            throw zio::message_error::create(3,"bad payload");
        }
        std::uint8_t* b = zframe_data(frame);
        size_t s = zframe_size(frame);
        m_payload.emplace_back(b, b+s);
        zframe_destroy(&frame);
    }
    zmsg_destroy(&msg);
}

void zio::Message::next(const payload_t& pl, granule_t gran)
{
    multiload_t ml;
    ml.push_back(pl);
    next(ml, gran);
}

void zio::Message::next(const multiload_t& pl, granule_t gran)
{
    if (gran == 0) {
        gran = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    m_header.coord.granule = gran;
    ++m_header.coord.seqno;
    m_payload = pl;

}