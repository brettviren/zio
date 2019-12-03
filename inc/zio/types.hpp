#ifndef ZIO_TYPES_HPP_SEEN
#define ZIO_TYPES_HPP_SEEN

#include <unordered_map>
#include <string>
#include <vector>

namespace zio {
    enum borc_t { bind, connect };

    /// A timeout in milliseconds
    typedef int timeout_t;
    /// Uniquely identify a peer
    typedef std::string uuid_t;
    /// A peer asserts a nickname
    typedef std::string nickname_t;
    /// Mapping from UUID to nickname
    typedef std::unordered_map<uuid_t, nickname_t> nickmap_t;
    /// A "header" is on in a set of key/value pairs asserted by a peer.
    typedef std::string header_key_t;
    typedef std::string header_value_t;
    typedef std::vector<key_t> keyset_t;
    // header key may not be unique in header set
    typedef std::pair<header_key_t, header_value_t> header_t;
    typedef std::vector<header_t> headerset_t;

    struct peer_info_t {
        nickname_t nick;
        headerset_t headers;
    };
    typedef std::unordered_map<uuid_t, peer_info_t> peerset_t;
    

    /// An address in ZeroMQ format, eg <transport>://<endpoint>
    typedef std::string address_t;


    /// All messages have a "level" indicating importance and implying
    /// frequency.
    namespace level {
        enum MessageLevel {
            reserved=0,
            trace,verbose,debug,info,summary,warning,error,fatal,
        };
    }
    const char* MessageLevelString[] = {
        "undefined",
        "trace","verbose","debug","info","summary","warning","error","fatal",
        0
    };



}
#endif

