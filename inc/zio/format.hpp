/**

   Each format by ZIO has a struct that represents it.

   This struct provides the data in a normalized way (std::string)
   with adapters to the C++ type that best represents the type.

 */

// fixme: add protobuf under conditional compilation

#ifndef ZIO_FORMAT_HPP_SEEN
#define ZIO_FORMAT_HPP_SEEN

#include "zio/types.hpp"

#include <string>
#include <vector>
#include <cstring>

namespace zio {

    typedef std::vector<std::uint8_t> byte_array_t;

    inline
    std::string tostring(const byte_array_t& buf) {
        return std::string(buf.begin(), buf.end());
    }
    inline
    json tosjson(const byte_array_t& buf) {
        std::string s = tostring(buf);
        return json::parse(s);
    }

    /*
      Each TYPE struct adds a constructor from one or more native
      representations to a byte_array_t and an operator() that returns
      the most fitting native type.  
     */

    struct Header {
        int level;
        std::string format, label;
        uint64_t origin, granule, seqno;
    };

    struct Format {
        virtual const char* type() const = 0;
        byte_array_t buffer;
        size_t size() const { return buffer.size(); }
        const uint8_t* data() const { return buffer.data(); }
    };


    struct BUFF : public Format {
        virtual const char* type() const { return "BUFF"; };
        BUFF(const char* str) {
            size_t size = strlen(str);
            buffer.insert(buffer.end(), str, str+size);
        }
        BUFF(const char* data, size_t size) {
            buffer.insert(buffer.end(), data, data+size);
        }
        BUFF(const std::string& s) {
            buffer.insert(buffer.end(), s.begin(), s.end());
        }
        std::string operator()() {
            return std::string(buffer.begin(), buffer.end());
        }
    };

    struct TEXT : public Format {
        virtual const char* type() const { return "TEXT"; };
        TEXT(const char* str) {
            size_t size = strlen(str);
            buffer.insert(buffer.end(), str, str+size);
        }
        TEXT(const char* data, size_t size) {
            buffer.insert(buffer.end(), data, data+size);
        }
        TEXT(const std::string& s) {
            buffer.insert(buffer.end(), s.begin(), s.end());
        }
        std::string operator()() {
            return std::string(buffer.begin(), buffer.end());
        }
    };


    struct JSON : public Format {
        virtual const char* type() const { return "JSON"; };
        JSON(const json& j) {
            const std::string s = j.dump();
            buffer.insert(buffer.end(), s.begin(), s.end());
        }
        json operator()() {
            std::string s(buffer.begin(), buffer.end());
            return json::parse(s);
        }
    };

    struct BSON : public Format {
        virtual const char* type() const { return "BSON"; };
        BSON(const json& j) {
            buffer = json::to_bson(j);
        }
        json operator()() {
            return json::from_bson(buffer);
        }
    };

    struct CBOR : public Format {
        virtual const char* type() const { return "CBOR"; };
        CBOR(const json& j) {
            buffer = json::to_cbor(j);

        }
        json operator()() {
            return json::from_cbor(buffer);
        }
    };

    struct MSGP : public Format {
        virtual const char* type() const { return "MSGP"; };
        MSGP(const json& j) {
            buffer = json::to_msgpack(j);
        }
        json operator()() {
            return json::from_msgpack(buffer);
        }
    };

    struct UBJS : public Format {
        virtual const char* type() const { return "UBJS"; };
        UBJS(const json& j) {
            buffer = json::to_ubjson(j);
        }
        json operator()() {
            return json::from_ubjson(buffer);
        }
    };
    
}

#endif
