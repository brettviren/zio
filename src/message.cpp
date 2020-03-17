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
    ss << "ZIO" << num << form;
    if (label.size() > 0) {
        ss << label;
    }
    return ss.str();
}

bool zio::PrefixHeader::loads(const std::string& p)
{
    const size_t siz = p.size();
    if (siz < 8) return false;
    if (p.substr(0,3) != "ZIO") return false;
    level = static_cast<zio::level::MessageLevel>(p[3]-'0');
    form = p.substr(4,4);
    label = p.substr(8);
    return true;
}        

zio::Message::Message()
    : m_rid(0)
{
}

zio::Message::Message(const header_t h, multipart_t&& pl)
    : m_header(h)
    , m_payload(std::move(pl))
    , m_rid(0)
{
}

zio::Message::Message(const header_t h)
    : m_header(h)
    , m_rid(0)
{
}

zio::Message::Message(const std::string& form, level::MessageLevel lvl)
    : m_header({{lvl,"    ",""},{0,0,0}})
    , m_rid(0)
{
    set_form(form);
}

zio::level::MessageLevel zio::Message::level() const
{
    return m_header.prefix.level;
}
void zio::Message::set_level(level::MessageLevel level) 
{
    m_header.prefix.level = level;
}
void zio::Message::set_label(const std::string& label)
{
    m_header.prefix.label = label;
}

zio::json zio::Message::label_object() const
{
    return json::parse(label());
}
void zio::Message::set_label_object(const zio::json& lobj)
{
    set_label(lobj.dump());
}


std::string zio::Message::form() const {
    return m_header.prefix.form;
}
void zio::Message::set_form(const std::string& form)
{
    m_header.prefix.form = "    ";
    const size_t maxsiz = 4;
    const size_t siz = std::min(form.size(), maxsiz);
    for (size_t ind=0; ind<siz; ++ind) {
        m_header.prefix.form[ind] = form[ind];
    }
}
std::string zio::Message::label() const {
    return m_header.prefix.label;
}

void zio::Message::set_coord(origin_t origin, granule_t gran)
{
    if (gran == 0) {
        auto tmp = std::chrono::system_clock::now().time_since_epoch();
        gran = std::chrono::duration_cast<std::chrono::microseconds>(tmp).count();
    }
    m_header.coord.granule = gran;    
    if (origin) {
        m_header.coord.origin = origin;
    }
}

zio::message_t zio::Message::encode() const
{
    auto mmsg = toparts();
    return mmsg.encode();
    // auto parts = toparts();
    // size_t size = 0;
    // for (auto& part : parts) {
    //     size += sizeof(int) + part.size();
    // }
    // zio::message_t ret(size);
    // std::uint8_t* out = ret.data<std::uint8_t>();
    // for (auto& part : parts) {
    //     int siz = part.size();
    //     memcpy(out, &siz, sizeof(int));
    //     out += sizeof(int);
    //     memcpy(out, part.data(), siz);
    //     out += siz;
    // }
    // if (m_rid>0) 
    //     ret.set_routing_id(m_rid);
    // return ret;
}

void zio::Message::decode(const zio::message_t& data)
{
    m_rid = data.routing_id();
    fromparts(zio::multipart_t::decode(data));

    // const std::uint8_t* dat = data.data<std::uint8_t>();
    // size_t tot = data.size();

    // zio::multipart_t mpmsg;

    // size_t ind=0;
    // while (ind < tot) {
    //     int siz=0;
    //     memcpy(&siz, dat+ind, sizeof(int));
    //     ind += sizeof(int);
    //     //zsys_debug("decoding size %d", siz);
    //     mpmsg.addmem(dat+ind, siz);
    //     ind += siz;
    // }
    // fromparts(mpmsg);           //  clears rid

    // m_rid = data.routing_id();
}
        
zio::multipart_t zio::Message::toparts() const
{
    zio::multipart_t mpmsg;

    std::string p = m_header.prefix.dumps();
    mpmsg.addmem(p.data(), p.size());
    mpmsg.addtyp(m_header.coord);
    for (const auto& spmsg: m_payload) {
        mpmsg.addmem(spmsg.data(), spmsg.size());
    }
    return mpmsg;
}

void zio::Message::fromparts(const zio::multipart_t& mpmsg)
{
    m_rid = 0;
    const size_t nparts = mpmsg.size();

    const auto& m0 = mpmsg[0];
    std::string p(static_cast<const char*>(m0.data()), m0.size());
    bool ok = m_header.prefix.loads(p);
    if (!ok) {
        throw std::runtime_error("failed to parse prefix from parts");
    }
    
    const auto& m1 = mpmsg[1];
    m_header.coord = *m1.data<zio::CoordHeader>();

    m_payload.clear();
    for (size_t ind=2; ind<nparts; ++ind) {
        const auto& m = mpmsg[ind];
        m_payload.addmem(m.data(), m.size());
    }
}
