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


    /// An address in ZeroMQ format, eg <transport>://<endpoint>
    typedef std::string address_t;

    /// A zio message payload
    typedef std::vector<std::uint8_t> payload_t;


}
#endif

