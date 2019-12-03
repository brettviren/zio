#ifndef ZIO_PRODUCER_HPP_SEEN
#define ZIO_PRODUCER_HPP_SEEN

#include "zio/socket.hpp"
#include <json.hpp>

namespace zio {

    using json = nlohmann::json;

    /** 

        A zio producer sends messages of a particular type via a fully
        bound/connected socket.

     */
    
    typedef struct { size_t size, char* buff } BUFF;
    typedef std::string TEXT;
    typedef nlohmann::json JSON;

    struct TimeGranule {
        uint64_t operator()() {
            return zclock_usec();
        }
    };

    template<GRANULE = TimeGranule>
    class Producer {
        std::shared_ptr<Socket> m_sock;
        uint64_t m_origin;
        GRANULE m_granule;
    public:
        Producer(uint64_t origin, int stype);
        Producer(uint64_t origin, std::shared_ptr<Socket>);

        std::shared_ptr<Socket> socket() { return m_sock; }

        // back levels semantics into method names

        template<typename TYPE>
        void trace(const TYPE& payload)   { (*this)(level::trace, payload); }

        template<typename TYPE>
        void verbose(const TYPE& payload) { (*this)(level::verbose, payload); }

        template<typename TYPE>
        void debug(const TYPE& payload)   { (*this)(level::debug, payload); }

        template<typename TYPE>
        void info(const TYPE& payload)    { (*this)(level::info, payload); }

        template<typename TYPE>
        void summary(const TYPE& payload) { (*this)(level::summary, payload); }

        template<typename TYPE>        
        void warning(const TYPE& payload) { (*this)(level::warning, payload); }

        template<typename TYPE>        
        void error(const TYPE& payload)   { (*this)(level::error, payload); }

        template<typename TYPE>        
        void fatal(const TYPE& payload)   { (*this)(level::fatal, payload); }


        template<typename TYPE>
        void operator()(level::MessageLevel lvl, const TYPE& payload);

        template<>
        void operator()(level::MessageLevel lvl, const TEXT& payload) {
            send(lvl, "TEXT", payload.size(), payload.data());
        }
        template<>
        void operator()(level::MessageLevel lvl, BUFF& payload) {
            send(lvl, "BUFF", payload.size, payload.data);
        }
        template<>
        void operator()(level::MessageLevel lvl, JSON& payload) {
            const std::string s = payload.dump();
            send(lvl, "JSON", s.size(), s.data());
        }

        void send(level::MessageLevel lvl, const std::string& format, size_t size, const char* data) {
            zmsg_t* msg = zmsg_new();
            zmsg_addstrf(msg, "ZIO%d%s%ld%ld", lvl, format.c_str(), m_origin, m_granule());
            zmsg_addmem(msg, data, size);
            zmsg_send(&msg, m_sock->socket());
        }                
            
    };


}

#endif

