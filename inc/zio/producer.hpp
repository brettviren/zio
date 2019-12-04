#ifndef ZIO_PRODUCER_HPP_SEEN
#define ZIO_PRODUCER_HPP_SEEN

#include "zio/socket.hpp"
#include "zio/types.hpp"
#include <json.hpp>

namespace zio {

    /** 

        A zio producer sends messages of a particular type via a fully
        bound/connected socket.

     */
    
    struct BUFF {
        size_t size{0};
        const char* data{0};
        BUFF(const char* s) : size{strlen(s)}, data{s} { }
        BUFF(const std::string& s) : size{s.size()}, data{s.data()} { }
        BUFF(size_t siz, const char* dat) : size(siz), data(dat) {}
    };

    typedef std::string TEXT;
    typedef nlohmann::json JSON;

    struct TimeGranule {
        granule_t operator()() {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
    };


    template<typename TYPE>
    class Producer {
        std::shared_ptr<Socket> m_sock;
        uint64_t m_origin;
        granule_func_t m_granule;
    public:
        Producer(uint64_t origin,
                 std::shared_ptr<Socket> sptr = std::make_shared<Socket>(ZMQ_PUB),
                 granule_func_t gf = TimeGranule())
            : m_origin(origin)
            , m_sock(sptr)
            , m_granule(gf) {}

        template<typename OTYPE>
        Producer(const Producer<OTYPE>& rhs)
            : m_origin(rhs.origin())
            , m_sock(rhs.socket())
            , m_granule(rhs.granuler()) {}

        ~Producer() {}

        std::shared_ptr<Socket> socket() const { return m_sock; }
        uint64_t origin() const { return m_origin; }
        granule_func_t granuler() const { return m_granule; }

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

        void sendfull(level::MessageLevel lvl, const std::string& format,
                      size_t size, const char* data) {
            zsys_debug("send: %d %s %ld", lvl, format.c_str(), size);
            zmsg_t* msg = zmsg_new();
            zmsg_addstrf(msg, "ZIO%d%s%ld%ld", lvl, format.c_str(), m_origin, m_granule());
            zmsg_addmem(msg, data, size);
            zmsg_send(&msg, m_sock->zsock());
        }                
            
        virtual void send(level::MessageLevel lvl, const TYPE& payload) {
            abort();
        }

    };

    template<>
    void Producer<TEXT>::send(level::MessageLevel lvl, const TEXT& payload) {
        this->sendfull(lvl, "TEXT", payload.size(), payload.data());
    }

    template<>
    void Producer<BUFF>::send(level::MessageLevel lvl, const BUFF& payload)  {
        this->sendfull(lvl, "BUFF", payload.size, payload.data);
    }

    template<>
    void Producer<JSON>::send(level::MessageLevel lvl, const JSON& payload)  {
        const std::string s = payload.dump();
        this->sendfull(lvl, "JSON", s.size(), s.data());
    }

}



#endif

