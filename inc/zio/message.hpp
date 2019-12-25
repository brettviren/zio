/**
   Functions to manage message data including send and receiving with
   a socket.

 */

#ifndef ZIO_MESSAGE_HPP_SEEN
#define ZIO_MESSAGE_HPP_SEEN

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
        std::string format{""};
        std::string label{""};

        std::string dumps() const;
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
      @brief a zio message

      A message object may be used to build up the message data and
      serializer to or parse from raw bytes.
     */
    class Message {
    public:
        typedef Header header_t;
        typedef std::vector<std::uint8_t> encoded_t;
        typedef std::vector<std::uint8_t> payload_t;
        typedef std::vector<payload_t> multiload_t;


        Message();
        Message(const encoded_t& data);
        Message(const header_t h, const multiload_t& pl = multiload_t());
        
        void set_level(level::MessageLevel level) {
            m_header.prefix.level = level;
        }
        void set_label(const std::string& label) {
            m_header.prefix.label = label;
        }
        std::string format() const {
            return m_header.prefix.format;
        }
        std::string label() const {
            return m_header.prefix.label;
        }

        /// prepare for sending, advance seqno, set granule (if 0 use
        /// time), origin (if 0 do not set).
        void set_coord(origin_t origin=0, granule_t gran=0);

        /// Reset self to empty message
        void clear();

        /// Encode message to flat array
        encoded_t encode() const;

        /// Set self based on encoded array
        void decode(const encoded_t& dat);

        /// Access header
        const header_t& header() const { return m_header; }

        /// Access payload(s)
        const multiload_t& payload() const { return m_payload; }
        multiload_t& payload() { return m_payload; }


    private:
        header_t m_header;
        multiload_t m_payload;
        
    };


}

#endif
