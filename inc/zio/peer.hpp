#ifndef ZIO_PEER_HPP_SEEN
#define ZIO_PEER_HPP_SEEN

#include <zyre.h>
#include <map>
#include <string>
#include <vector>

namespace zio {

    /// A peer asserts a nickname
    typedef std::string nickname_t;
    /// Uniquely identify a peer
    typedef std::string uuid_t;

    /// A "header" is on in a set of key/value pairs asserted by a peer.
    typedef std::string header_key_t;
    typedef std::string header_value_t;

    // header key must be unique in a header set
    typedef std::pair<header_key_t, header_value_t> header_t;
    typedef std::map<header_key_t, header_value_t> headerset_t;

    struct peer_info_t
    {
        nickname_t nick{""};
        headerset_t headers;

        // return a new mapping with entries of matching prefix.
        // Eg, {"a":"blech", "a.b":"foo", "a.b.c":"bar"}.
        // Branched on "a.b." returns: {c:"bar"}.
        // Branched on "a.b"  returns: {"":"foo", ".c":"bar"}
        headerset_t branch(const std::string& prefix);
    };

    typedef std::map<uuid_t, peer_info_t> peerset_t;

    /*!
      @brief Peer at the network to discover peers and advertise self.

      This is a C++ interface to ZeroMQ's Zyre which adds some memory
      of peers seen and ways to iterate on their Zyre headers.
    */
    class Peer
    {
      public:
        /// A timeout in milliseconds
        typedef int timeout_t;

        ~Peer();

        /// Advertise own nickname and headers
        Peer(const nickname_t& nickname, const headerset_t& headers = {},
             bool verbose = false);

        /// Turn on verbose debugging of the underlying Zyre actor.
        void set_verbose(bool verbose = true);

        /// Get our nickname.
        const nickname_t nickname() { return m_nick; }

        /// @brief Poll the network for updates, timeout in msec.
        ///
        /// Return true if an even was from the nework processed.  Use
        /// timeout=-1 to wait until an event if received.
        bool poll(timeout_t timeout = 0);

        /// Continually poll until all queued zyre messages are processed.
        void drain();

        /// @brief Wait for a peer of a given nickname to be discovered.
        ///
        /// Return UUID if found, empty string if timeout occurs.
        /// Note, multiple peers may share the same nickname.
        std::vector<uuid_t> waitfor(const nickname_t& nickname,
                                    timeout_t timeout = -1);

        /// @brief Wait until a specific peer has left the network.
        ///
        /// If it is already gone, return immediately or in any case
        /// no longer than the timeout.
        void waituntil(const uuid_t& uuid, timeout_t timeout = -1);

        /// @brief Return known peers as map from UUID to nickname.
        ///
        /// This will return new values on subsequent calls as peers
        /// enter and exit the network.
        const peerset_t& peers();

        /// Return info about peer.  If unknown, return default structure.
        peer_info_t peer_info(const uuid_t& uuid);

        /// Return true if peer has been seen ENTER the network and not yet seen
        /// to EXIT.
        bool isknown(const uuid_t& uuid);

        /// Return all UUIDs with matching nickname.
        std::vector<uuid_t> nickmatch(const nickname_t& nick);

      private:
        std::string m_nick;
        bool m_verbose;
        zyre_t* m_zyre;

        peerset_t m_known_peers;
    };
}  // namespace zio

#endif
