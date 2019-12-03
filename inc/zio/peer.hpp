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
        peerset_t peers() const;

        /// Return true if peer is active on the network right now.
        bool seen(const uuid_t& uuid);

        /// Return all UUIDs with matching nickname.
        std::vector<uuid_t> nickmatch(const nickname_t& nick);

    private:
        zyre_t* m_zyre;

        peerset_t m_known_peers;
    };
}

zio::Peer::Peer(const nickname_t& nickname, const headerset_t& headers)
{
    m_zyre = zyre_new(nickname.c_str());
    if (!m_zyre) {
        std::string s = "Peer(" + nickname + ") failed to create zyre";
        throw std::runtime_error(s);
    }
    for (const auto& header : headers) {
        zyre_set_header(m_zyre, header.first.c_str(), "%s", header.second.c_str());
    }
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
        if (streq(event_type, "ENTER")) {
            uuid_t uuid = zyre_event_peer_uuid(event);
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
            }
            m_known_peers[uuid] = pi;
            zlist_destroy (&keys);
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
            if (timeout == -1 or zclock_usecs() / 1000 - start >= timeout) {
                continue;
            }
        }

        break;
    }
    zpoller_destroy(&poller);   // fixme: retain poller?
    return got_one;
}


std::vector<zio::uuid_t> zio::Peer::nickmatch(const nickname_t& nickname)
{
    std::vector<uuid_t> ret;
    for (const auto& pp : m_known_peers) {
        if (pp.second.nick == nickname) {
            ret.push_back(pp.first);
        }
    }
    return ret;
}

std::vector<zio::uuid_t> zio::Peer::waitfor(const nickname_t& nickname, timeout_t timeout)
{
    const int64_t start = zclock_usecs() / 1000;
    std::vector<uuid_t> maybe = nickmatch(nickname);

    while (maybe.empty()) {
        poll(timeout);
        maybe = nickmatch(nickname);
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
