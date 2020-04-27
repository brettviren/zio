/*! An actor is a function running in a thread with a control link to
    the parent thread.

    This actor implements the "mini protocol" for startup/shutdown used by CZMQ
*/

#ifndef ZIO_ACTOR_HPP_SEEN
#define ZIO_ACTOR_HPP_SEEN

#include "zio/cppzmq.hpp"

#include <thread>

namespace zio {

    /*! Return a linked pair of PAIR sockets.
     *
     * First is bound and second is connected via inproc://
     */
    inline std::pair<socket_t, socket_t> create_linked_pairs(context_t& ctx)
    {
        std::pair<socket_t, socket_t> ret{socket_t(ctx, socket_type::pair),
                                          socket_t(ctx, socket_type::pair)};

        std::stringstream ss;
        ss << "inproc://link-" << std::hex << ret.first.handle() << "-"
           << ret.second.handle();
        std::string addr = ss.str();
        ret.first.bind(addr.c_str());
        ret.second.connect(addr.c_str());
        return ret;
    }

    /*! Spawn a function in a thread and communicate over a link.

      The actor function must take a socket and zero or more optional
      arguments such as:

          void func(socket_t& sock, ...);

          zio::zactor_t actor(ctx, func, ...);
          actor.link().send(...);

      The socket passed in to the actor function is one end of a
      bidirection link shared with the application thread.  The
      application thread may get the other end of that link by calling
      zio::zactor_t::link().

      The actor function is expected to participate in a protocol on
      the link which is compatible with the requirements of CZMQ
      zactor_fn actor functions:

      - The function body shall send to the link an initial "ready
        signal" message (message content does not matter).

      - The parent thread shall block until receiving an initial
        message from the link.

      - The function body shall expect a "termination message" (5
        bytes, content "$TERM") at any time from the link and shall
        exit shortly upon receiving it.

      - Upon successfull send of "termination message" the parent
        thread shall block until the actor function exits.

      - If send of "termination message" fails, the actor function
        shall be presumed to have already exited.

     */
    class zactor_t
    {
      public:
        // Template constructor and not class to perform type erasure
        // of the function type.
        template <typename Func, typename... Args>
        zactor_t(context_t& ctx, Func fn, Args... args)
        {
            socket_t asock;
            std::tie(asock, _sock) = create_linked_pairs(ctx);

            _thread = std::thread(
                [fn = std::forward<Func>(fn)](socket_t asock, Args... args) {
                    fn(asock, std::forward<Args>(args)...);
                    // send notice of exit down link
                    asock.send(message_t{}, send_flags::none);
                },
                std::move(asock), std::forward<Args>(args)...);
            startup();
        }

        ~zactor_t()
        {
            shutdown();
            _thread.join();
        }

        socket_ref link() { return _sock; }

      private:
        void startup()
        {
            // The default contract with the actor function is that it
            // shall notify us with a "signal" message that we may
            // continue.
            message_t rmsg;
            auto res = link().recv(rmsg, recv_flags::none);
            res = {};  // don't care
        }

        void shutdown()
        {
            // The default contract with the actor function is we will
            // notify it to terminate.  If sending that message is
            // successful we wait for the built-in confirmation
            // message.
            auto sres =
                link().send(message_t("$TERM", 5), send_flags::dontwait);
            if (sres) {
                message_t rmsg;
                auto res = link().recv(rmsg, recv_flags::none);
                res = {};  // don't care
            }
        }

      private:
        socket_t _sock;
        std::thread _thread;
    };
}  // namespace zio

#endif
