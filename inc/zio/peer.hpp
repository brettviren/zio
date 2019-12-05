/** 

    A C++ class interface adding peer info caching and a "wait for
    peer to show up" method to ZeroMQ's Zyre.

 */

#ifndef ZIO_PEER_HPP_SEEN
#define ZIO_PEER_HPP_SEEN

#include "zio/types.hpp"

#include <zyre.h>
#include <czmq.h>

namespace zio {

    struct peer_info_t {
        nickname_t nick{""};
        headerset_t headers;

        std::vector<header_value_t> lookup(const header_key_t& key) {
            std::vector<header_value_t> ret;
            for (const auto& one : headers) {
                if (one.first == key) {
                    ret.push_back(one.second);
                }
            }
            return ret;
        }

        std::vector<header_value_t> match(const header_key_t& key, const std::string& prefix) {
            std::vector<header_value_t> ret;
            const auto psiz = prefix.size();
            for (const auto& val : lookup(key)) {
                const auto vsiz = val.size();
                if (vsiz < psiz) continue;
                if (val.substr(0,psiz) != prefix) continue;
                ret.push_back(val);
            }
            return ret;
        }

    };
    typedef std::unordered_map<uuid_t, peer_info_t> peerset_t;


    /// Peer at the network to discover peers and advertise self.
    class Peer {
    public:
        ~Peer();

        /// Advertise own nickname and headers
        Peer(const nickname_t& nickname,
             const headerset_t& headers, bool verbose=false);


        const nickname_t nickname() { return m_nick; }

        /// Poll network for updates, timeout in msec.  Return true if
        /// an even was from the nework processed.  Use timeout=-1 to
        /// wait until an event if received.
        bool poll(timeout_t timeout = 0);

        /// Continually poll until all queued zyre messages are processed.
        void drain();

        /// Wait for a peer of a given nickname to be discovered.
        /// Return UUID if found, empty string if timeout occurs.
        std::vector<uuid_t> waitfor(const nickname_t& nickname, timeout_t timeout = -1);

        /// Return known peers as map from UUID to nickname.  This
        /// will return new values on subsequent calls as peers enter
        /// and exit the network.
        const peerset_t& peers();

        /// Return info about peer.  If unknown, return default structure.
        peer_info_t peer_info(const uuid_t& uuid);

        /// Return true if peer has been seen ENTER the network and
        /// not yet seen to EXIT.
        bool isknown(const uuid_t& uuid);

        /// Return all UUIDs with matching nickname.
        std::vector<uuid_t> nickmatch(const nickname_t& nick);

    private:
        std::string m_nick;
        bool m_verbose;
        zyre_t* m_zyre;

        peerset_t m_known_peers;
    };
}

#endif
