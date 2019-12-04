#ifndef ZIO_PRODUCER_HPP_SEEN
#define ZIO_PRODUCER_HPP_SEEN

#include "zio/socket.hpp"
#include "zio/types.hpp"

namespace zio {

    /** 

        A zio producer sends messages of a particular type via a fully
        bound/connected socket.

     */
    
    template<typename TYPE, typename GRANULE = TimeGranule>
    class Producer {
        std::shared_ptr<Socket> m_sock;
        uint64_t m_origin;
        GRANULE m_granule;
    public:
        Producer(uint64_t origin, int stype)
            : m_sock(std::make_shared<Socket>(stype))
            , m_origin(origin) { }

        Producer(uint64_t origin, std::shared_ptr<Socket> sptr)
                : m_sock(sptr)
                , m_origin(origin) { }

        template<typename OPROD>
        Producer(const OPROD& other)
            : m_sock(other.socket())
            , m_origin(other.origin()) { }

        ~Producer() {}

        std::shared_ptr<Socket> socket() const { return m_sock; }
        uint64_t origin() const { return m_origin; }

        // bake levels semantics into method names
        void trace(const TYPE& payload)   { this->send(level::trace, payload); }
        void verbose(const TYPE& payload) { this->send(level::verbose, payload); }
        void debug(const TYPE& payload)   { this->send(level::debug, payload); }
        void info(const TYPE& payload)    { this->send(level::info, payload); }
        void summary(const TYPE& payload) { this->send(level::summary, payload); }
        void warning(const TYPE& payload) { this->send(level::warning, payload); }
        void error(const TYPE& payload)   { this->send(level::error, payload); }
        void fatal(const TYPE& payload)   { this->send(level::fatal, payload); }


        void operator()(level::MessageLevel lvl, const TYPE& payload) {
            this->send(lvl, payload);
        }


        void sendfull(level::MessageLevel lvl, const std::string& format, size_t size, const char* data) {
            zsys_debug("send: %d %s %ld", lvl, format.c_str(), size);
            zmsg_t* msg = zmsg_new();
            zmsg_addstrf(msg, "ZIO%d%s%ld%ld", lvl, format.c_str(), m_origin, m_granule());
            zmsg_addmem(msg, data, size);
            zmsg_send(&msg, m_sock->zsock());
        }                
            
        void send(level::MessageLevel lvl, const TEXT& payload) {
            this->sendfull(lvl, "TEXT", payload.size(), payload.data());
        }
        void send(level::MessageLevel lvl, const BUFF& payload) {
            this->sendfull(lvl, "BUFF", payload.size, payload.data);
        }
        void send(level::MessageLevel lvl, const JSON& payload) {
            const std::string s = payload.dump();
            this->sendfull(lvl, "JSON", s.size(), s.data());
        }


    };




}



#endif

