/** 

    A C++ class interface adding peer header caching to ZeroMQ's Zyre.

 */

#ifndef ZIO_PEER_HPP_SEEN
#define ZIO_PEER_HPP_SEEN

#include "zio/types.hpp"

#include <zyre.h>
#include <czmq.h>

namespace zio {

    /// Peer at the network to discover peers and advertise self.
    class Peer {
    public:
        /// Advertise own nickname and headers
        Peer(const nickname_t& nickname,
             const headerset_t& headers);
        ~Peer();
        
        const nickname_t nickname() { return m_nick; }

        /// Poll network for updates, timeout in msec.  Return true if
        /// an even was from the nework processed.  Use timeout=-1 to
        /// wait until an event if received.
        bool poll(timeout_t timeout = 0);

        /// Wait for a peer of a given nickname to be discovered.
        /// Return UUID if found, empty string if timeout occurs.
        std::vector<uuid_t> waitfor(const nickname_t& nickname, timeout_t timeout = -1);

        /// Return known peers as map from UUID to nickname.  This
        /// will return new values on subsequent calls as peers enter
        /// and exit the network.
        const peerset_t& peers() const { return m_known_peers; }

        /// Return true if peer is active on the network right now.
        bool seen(const uuid_t& uuid);

        /// Return all UUIDs with matching nickname.
        std::vector<uuid_t> nickmatch(const nickname_t& nick);

    private:
        std::string m_nick;
        zyre_t* m_zyre;

        peerset_t m_known_peers;
    };
}

zio::Peer::Peer(const nickname_t& nickname, const headerset_t& headers)
    : m_nick(nickname), m_zyre(nullptr)
{
    m_zyre = zyre_new(m_nick.c_str());
    if (!m_zyre) {
        std::string s = "Peer(" + m_nick + ") failed to create zyre";
        throw std::runtime_error(s);
    }
    // debug
    //zyre_set_verbose(m_zyre);
    for (const auto& header : headers) {
        zyre_set_header(m_zyre, header.first.c_str(), "%s", header.second.c_str());
    }
    // fixme: starting this here negates the user doing various useful
    // things with the zyre node.
    // zsys_debug("starting zyre node: '%s' on '%s'", m_nick.c_str(), nic);
    zyre_start(m_zyre);
}
zio::Peer::~Peer()
{
    zyre_destroy(&m_zyre);
}

bool zio::Peer::poll(timeout_t timeout)
{
    const int64_t start = zclock_usecs() / 1000;

    bool got_one = false;
    zpoller_t* poller = zpoller_new(zyre_socket(m_zyre), NULL);
    while (true) {
        void* which = zpoller_wait(poller, timeout);
        if (!which) {
            break;              // timeout
        }
        zyre_event_t *event = zyre_event_new (m_zyre);
        const char* event_type = zyre_event_type(event);
        //zsys_debug("%s: event '%s'", m_nick.c_str(), event_type);
        //zyre_event_print(event);
        if (streq(event_type, "ENTER")) {
            uuid_t uuid = zyre_event_peer_uuid(event);
            //zsys_debug("%s: see peer %s", m_nick.c_str(),uuid.c_str());
            peer_info_t pi;
            pi.nick = zyre_event_peer_name(event);
            zhash_t* hh = zyre_event_headers(event);
            zlist_t* keys = zhash_keys(hh);
            void* cursor = zlist_first(keys);
            while (cursor) {
                void* vptr = zhash_lookup(hh, static_cast<char*>(cursor));
                header_key_t key = static_cast<char*>(cursor);
                header_value_t val = static_cast<char*>(vptr);
                pi.headers.push_back(header_t(key,val));
                //zsys_debug("%s: add header %s:%s", m_nick.c_str(),key.c_str(),val.c_str());
                cursor = zlist_next(keys);
            }
            m_known_peers[uuid] = pi;
            zlist_destroy (&keys);
            //zsys_debug("%s: add %s (%s), knows %ld peers",
            //           m_nick.c_str(), pi.nick.c_str(), uuid.c_str(), m_known_peers.size());
            got_one = true;
        }
        else
        if (streq(event_type, "EXIT")) {
            uuid_t uuid = zyre_event_peer_uuid(event);
            peerset_t::iterator maybe = m_known_peers.find(uuid);
            if (maybe != m_known_peers.end()) {
                m_known_peers.erase(maybe);
            }
            got_one = true;
        }
        zyre_event_destroy(&event);

        // Ignore other events but keep going if we've not yet reached timeout
        if (!got_one) {
            if (timeout < 0) {
                //zsys_debug("%s: poll again with infinite wait", m_nick.c_str());
                continue;
            }
            auto timeleft = zclock_usecs() / 1000 - start - timeout;
            if (timeleft > 0) {
                //zsys_debug("%s: poll again with %ld msec left", m_nick.c_str(), timeleft);
                continue;
            }
        }
        //zsys_debug("%s: poll done", m_nick.c_str());
        break;
    }
    zpoller_destroy(&poller);   // fixme: retain poller?
    return got_one;
}


std::vector<zio::uuid_t> zio::Peer::nickmatch(const nickname_t& nickname)
{
    std::vector<uuid_t> ret;
    for (const auto& pp : m_known_peers) {
        //zsys_debug("check nick '%s' against '%s'", nickname.c_str(), pp.second.nick.c_str());
        if (pp.second.nick == nickname) {
            ret.push_back(pp.first);
        }
    }
    //zsys_debug("%s: found %ld instances of %s", m_nick.c_str(), ret.size(), nickname.c_str());
    return ret;
}

std::vector<zio::uuid_t> zio::Peer::waitfor(const nickname_t& nickname, timeout_t timeout)
{
    const int64_t start = zclock_usecs() / 1000;
    std::vector<uuid_t> maybe = nickmatch(nickname);

    while (maybe.empty()) {
        //zsys_debug("%s: waiting for %s", m_nick.c_str(), nickname.c_str());
        poll(timeout);
        maybe = nickmatch(nickname);
        //zsys_debug("%s: poll done after %ld ms, have %ld",
        //           m_nick.c_str(), zclock_usecs() / 1000 - start, maybe.size());
        if (timeout <= 0) {
            break;
        }
        if (zclock_usecs() / 1000 - start >= timeout) {
            break;
        }
    }
    return maybe;
}

#endif
