#ifndef ZIO_TYPES_HPP_SEEN
#define ZIO_TYPES_HPP_SEEN

#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>
#include <functional>

#include "zio/json.hpp"

namespace zio {
    // The great Nlohmann's JSON.
    using json = nlohmann::json;

    /// A peer asserts a nickname
    typedef std::string nickname_t;
    /// Uniquely identify a peer
    typedef std::string uuid_t;
    /// Mapping from UUID to nickname
    typedef std::unordered_map<uuid_t, nickname_t> nickmap_t;
    /// A "header" is on in a set of key/value pairs asserted by a peer.
    typedef std::string header_key_t;
    typedef std::string header_value_t;
    typedef std::vector<key_t> keyset_t;
    // header key may not be unique in header set
    typedef std::pair<header_key_t, header_value_t> header_t;
    typedef std::vector<header_t> headerset_t;


    /// An address in ZeroMQ format, eg <transport>://<endpoint>
    typedef std::string address_t;

    /// A zio message payload
    typedef std::vector<std::uint8_t> payload_t;


}
#endif

