/**

   Each format by ZIO has a struct that represents it.

   This struct provides the data in a normalized way (std::string)
   with adapters to the C++ type that best represents the type.

 */

#ifndef ZIO_FORMAT_HPP_SEEN
#define ZIO_FORMAT_HPP_SEEN

#include <string>
#include <vector>
#include <cstring>
#include <json.hpp>

namespace zio {

    typedef std::vector<std::uint8_t> byte_array_t;

    /*
      Each TYPE struct adds a constructor from one or more native
      representations to a byte_array_t and an operator() that returns
      the most fitting native type.  
     */

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
        JSON(const nlohmann::json& j) {
            const std::string s = j.dump();
            buffer.insert(buffer.end(), s.begin(), s.end());
        }
        nlohmann::json operator()() {
            std::string s(buffer.begin(), buffer.end());
            return nlohmann::json::parse(s);
        }
    };

    struct BSON : public Format {
        virtual const char* type() const { return "BSON"; };
        BSON(const nlohmann::json& j) {
            buffer = nlohmann::json::to_bson(j);
        }
        nlohmann::json operator()() {
            return nlohmann::json::from_bson(buffer);
        }
    };

    struct CBOR : public Format {
        virtual const char* type() const { return "CBOR"; };
        CBOR(const nlohmann::json& j) {
            buffer = nlohmann::json::to_cbor(j);

        }
        nlohmann::json operator()() {
            return nlohmann::json::from_cbor(buffer);
        }
    };

    struct MSGP : public Format {
        virtual const char* type() const { return "MSGP"; };
        MSGP(const nlohmann::json& j) {
            buffer = nlohmann::json::to_msgpack(j);
        }
        nlohmann::json operator()() {
            return nlohmann::json::from_msgpack(buffer);
        }
    };

    struct UBJS : public Format {
        virtual const char* type() const { return "UBJS"; };
        UBJS(const nlohmann::json& j) {
            buffer = nlohmann::json::to_ubjson(j);
        }
        nlohmann::json operator()() {
            return nlohmann::json::from_ubjson(buffer);
        }
    };
    
}

#endif
