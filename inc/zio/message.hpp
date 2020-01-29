/**
   Functions to manage message data including send and receiving with
   a socket.

 */

#ifndef ZIO_MESSAGE_HPP_SEEN
#define ZIO_MESSAGE_HPP_SEEN

#include "zio/interned.hpp"

#include <vector>
#include <string>

namespace zio {

    namespace level {
        enum MessageLevel {
            undefined=0,
            trace,verbose,debug,info,summary,warning,error,fatal,
        };

        const char* name(MessageLevel lvl);
    }

    struct PrefixHeader {
        level::MessageLevel level{level::undefined};
        std::string form{"    "}; // we keep this exactly length 4
        std::string label{""};

        std::string dumps() const;
        // Return false if s can not be parsed as a prefix header
        bool loads(const std::string& s);
    };

    typedef uint64_t origin_t;
    typedef uint64_t granule_t;
    typedef uint64_t seqno_t;

    struct CoordHeader {
        origin_t origin{0};
        granule_t granule{0};
        seqno_t seqno{0};
    };
    struct Header {
        PrefixHeader prefix;
        CoordHeader coord;
    };

    /*!
      @brief a ZIO message

      This adds semantic methods over a zio::multipart_t message.

      First part holds the prefix header, second the coord header.
      Optional following parts are payload.

     */
    class Message {
    public:
        typedef Header header_t;
        typedef uint32_t routing_id_t;

        Message();
        Message(const std::string& form, level::MessageLevel lvl = level::undefined);
        Message(const header_t h);
        Message(const header_t h, multipart_t&& pl);
        
        level::MessageLevel level() const;
        void set_level(level::MessageLevel level);
        std::string form() const;
        void set_form(const std::string& form);
        std::string label() const;
        void set_label(const std::string& label);

        const PrefixHeader& prefix() const { return m_header.prefix; }
        const CoordHeader& coord() const { return m_header.coord; }
        origin_t origin() const { return m_header.coord.origin; }
        granule_t granule() const { return m_header.coord.granule; }
        seqno_t seqno() const { return m_header.coord.seqno; }

        /// Prepare for sending, advance seqno automatically, set
        /// granule (if 0 use time), origin (if 0, leave as is).
        void set_coord(origin_t origin=0, granule_t gran=0);

        /// Explicit set
        void set_seqno(int seqno) { m_header.coord.seqno = seqno; }

        /// Encode self to single-part message.  If self has a routing
        /// ID, it will be set on the produced message.
        message_t encode() const;

        /// Set self based on encoded single-part message.  If it has
        /// a routing ID, it will be kept.
        void decode(const message_t& dat);

        /// Set self from multipart.  Nullifyies routing ID
        void fromparts(const multipart_t& allparts);

        /// Serialize self to multipart
        multipart_t toparts() const;

        /// Access payload(s)
        const multipart_t& payload() const { return m_payload; }
        void clear_payload() { m_payload.clear(); }
        void add(message_t&& spmsg) { m_payload.add(std::move(spmsg)); }

        /// Return routing ID if we have one
        routing_id_t routing_id() const { return m_rid; }
        
        /// Set routing ID
        void set_routing_id(routing_id_t rid) { m_rid = rid; }

        /// FIXME: how to handle ROUTER addressing?

    private:
        header_t m_header;
        multipart_t m_payload;
        routing_id_t m_rid;
        
    };


}

#endif
