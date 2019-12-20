/**
   Functions to mange message data including send and receiving with a
   socket.

 */

#ifndef ZIO_MESSAGE_HPP_SEEN
#define ZIO_MESSAGE_HPP_SEEN

#include <vector>
#include <string>

namespace zio {

    struct PrefixHeader {
        int level{0};
        std::string format{""}, label{""};

        std::string dumps() const;
    };
    struct CoordHeader {
        uint64_t origin{0}, granule{0}, seqno{0};
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
        Message(const header_t h, const multiload_t& pl = multiload_t());
        
        /// Reset self to empty message
        void clear();

        /// Encode message to flat array
        encoded_t encode() const;

        /// Set self based on encoded array
        void decode(const encoded_t& dat);

        /// Access header
        const header_t& header() const;

        /// Access payload(s)
        const multiload_t& payload() const;


    private:
        header_t m_header;
        multiload_t m_playload;
        
    };


}

#endif
