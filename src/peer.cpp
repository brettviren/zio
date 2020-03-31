#include "zio/peer.hpp"
#include "zio/logging.hpp"

zio::headerset_t zio::peer_info_t::branch(const std::string& prefix)
{
    const size_t len = prefix.size();
    headerset_t ret;
    for (const auto& hh : headers) {
        if (hh.first.substr(0,len) != prefix) {
            continue;
        }
        ret[hh.first.substr(len)] = hh.second;
    }
    return ret;
}

zio::Peer::Peer(const nickname_t& nickname, const headerset_t& headers,
                bool verbose)
    : m_nick(nickname), m_verbose(verbose), m_zyre(nullptr)
{
    m_zyre = zyre_new(m_nick.c_str());
    if (!m_zyre) {
        std::string s = "Peer(" + m_nick + ") failed to create zyre";
        throw std::runtime_error(s);
    }
    if (m_verbose) {
        zyre_set_verbose(m_zyre);
    }
    for (const auto& header : headers) {
        if (verbose)
            zio::debug("[peer {}]: header {} = {}",
                       m_nick,
                       header.first, header.second);
        zyre_set_header(m_zyre, header.first.c_str(), "%s", header.second.c_str());
    }
    // fixme: starting this here negates the user doing various useful
    // things with the zyre node.
    int rc = zyre_start(m_zyre);
    if (rc < 0) {
        throw std::runtime_error("failed to start zyre");
    }
}
zio::Peer::~Peer()
{
    zio::debug("[peer {}]: stop and destroy", m_nick);
    zyre_stop(m_zyre);
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
            zio::debug("[peer {}]: poll timeout", m_nick);
            break;              // timeout
        }
        zyre_event_t *event = zyre_event_new (m_zyre);
        if (!event) {
            zio::debug("[peer {}]: no zyre event", m_nick);
            break;              // timeout
        }
        const char* event_type = zyre_event_type(event);
        // zio::debug("[peer {}]: poll event '{}'",
        //            m_nick.c_str(), event_type);
        //zyre_event_print(event);
        if (streq(event_type, "ENTER")) {
            uuid_t uuid = zyre_event_peer_uuid(event);
            zio::debug("[peer {}]: poll ENTER peer \"{}\"", m_nick, uuid);
            peer_info_t pi;
            pi.nick = zyre_event_peer_name(event);
            zhash_t* hh = zyre_event_headers(event);
            zlist_t* keys = zhash_keys(hh);
            void* cursor = zlist_first(keys);
            while (cursor) {
                void* vptr = zhash_lookup(hh, static_cast<char*>(cursor));
                header_key_t key = static_cast<char*>(cursor);
                header_value_t val = static_cast<char*>(vptr);
                pi.headers[key] = val;
                zio::debug("[peer {}]: poll add {} header {}={}",
                           m_nick, pi.nick, key, val);
                cursor = zlist_next(keys);
            }
            m_known_peers[uuid] = pi;
            zlist_destroy (&keys);
            zio::debug("[peer {}]: poll add \"{}\" ({}), know {}",
                       m_nick, pi.nick, uuid, m_known_peers.size());
            got_one = true;
        }
        else
        if (streq(event_type, "EXIT")) {
            uuid_t uuid = zyre_event_peer_uuid(event);
            peerset_t::iterator maybe = m_known_peers.find(uuid);
            if (maybe != m_known_peers.end()) {
                m_known_peers.erase(maybe);
            }
            zio::debug("[peer {}]: poll remove {}, know {}",
                       m_nick, uuid, m_known_peers.size());
            got_one = true;
        }
        zyre_event_destroy(&event);

        // Ignore other events but keep going if we've not yet reached timeout
        if (!got_one) {
            if (timeout < 0) {
                zio::debug("[peer {}]: poll again with infinite wait",
                           m_nick);
                continue;
            }
            auto timeleft = zclock_usecs() / 1000 - start - timeout;
            if (timeleft > 0) {
                zio::debug("[peer {}]: poll again with {} msec left",
                           m_nick, timeleft);
                continue;
            }
        }
        //zio::debug("[peer {}]: poll done", m_nick.c_str());
        break;
    }
    zpoller_destroy(&poller);   // fixme: retain poller?
    return got_one;
}


std::vector<zio::uuid_t> zio::Peer::nickmatch(const nickname_t& nickname)
{
    std::vector<uuid_t> ret;
    for (const auto& pp : m_known_peers) {
        zio::debug("[peer {}]: check nick '{}' against '{}'",
                   m_nick, nickname, pp.second.nick);
        if (pp.second.nick == nickname) {
            ret.push_back(pp.first);
        }
    }
    zio::debug("[peer {}]: nickmatch found {} instances of {}",
               m_nick, ret.size(), nickname);
    return ret;
}

std::vector<zio::uuid_t> zio::Peer::waitfor(const nickname_t& nickname,
                                            timeout_t timeout)
{
    const int64_t start = zclock_usecs() / 1000;
    std::vector<uuid_t> maybe = nickmatch(nickname);

    while (maybe.empty()) {
        zio::debug("[peer {}]: waitfor peer \"{}\" to come online",
                   m_nick, nickname);
        poll(timeout);
        maybe = nickmatch(nickname);
        zio::debug("[peer {}]: waitfor see \"{}\" after {} ms, have {} match",
                   m_nick, nickname,
                   zclock_usecs() / 1000 - start,
                   maybe.size());

        if (maybe.size()) {
            break;
        }
        if (timeout < 0) {
            continue;
        }
        if (zclock_usecs() / 1000 - start >= timeout) {
            break;
        }
    }
    return maybe;
}

void zio::Peer::drain()
{
    while (poll()) {};
}

bool zio::Peer::isknown(const uuid_t& uuid)
{
    drain();
    peerset_t::iterator maybe = m_known_peers.find(uuid);
    return maybe != m_known_peers.end();
}

const zio::peerset_t& zio::Peer::peers()
{
    drain();
    return m_known_peers;
}
zio::peer_info_t zio::Peer::peer_info(const uuid_t& uuid)
{
    drain();
    peerset_t::iterator maybe = m_known_peers.find(uuid);
    if (maybe == m_known_peers.end()) {
        zio::debug("[peer {}]: failed to find peer {} out of {}",
                   m_nick, uuid, m_known_peers.size());
        return peer_info_t{};
    }
    return maybe->second;
}

void zio::Peer::set_verbose(bool verbose)
{
    m_verbose = verbose;
    if (m_verbose and m_zyre) {
        // note, zyre's verbose can't be turned off.
        zyre_set_verbose(m_zyre);
    }

}
