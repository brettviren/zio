#include "zio/stopwatch.hpp"
#include "zio/logging.hpp"
#include "zio/actor.hpp"
#include "zio/main.hpp"

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <deque>

void nofree(void *data, void * /*hint*/) { /* no op */ }
void dofree(void *data, void * /*hint*/) { free(data); }

int main(int argc, char *argv[])
{
    zio::init_all();
    if (argc != 3) { return -1; }
    const size_t siz = atol(argv[1]);
    const size_t num = atol(argv[2]);

    zio::Stopwatch sw;
    zio::info("start: {} bytes, {} count", siz, num);
    {
        sw.start();
        for (size_t ind = 0; ind < num; ++ind) {
            void *ptr = malloc(siz);
            free(ptr);
        }
        const double hz = sw.hz(num);
        zio::info("malloc: {:.3f} MHz, {:.3f} GB/s", 1e-6 * hz,
                  1e-9 * siz * hz);
    }
    {
        sw.restart();
        for (size_t ind = 0; ind < num; ++ind) {
            void *ptr = malloc(siz);
            memset(ptr, '\0', siz);
            free(ptr);
        }
        const double hz = sw.hz(num);
        zio::info("memset: {:.3f} MHz, {:.3f} GB/s", 1e-6 * hz,
                  1e-9 * siz * hz);
    }
    {
        sw.restart();
        void *src = malloc(siz);
        void *dst = malloc(siz);
        for (size_t ind = 0; ind < num; ++ind) { memcpy(dst, src, siz); }
        free(src);
        free(dst);
        const double hz = sw.hz(num);
        zio::info("memcpy: {:.3f} MHz, {:.3f} GB/s", 1e-6 * hz,
                  1e-9 * siz * hz);
    }

    {
        std::vector<char> dat(siz);
        std::deque<std::vector<char> > queue;
        sw.restart();
        for (size_t ind = 0; ind < num; ++ind) {
            queue.push_back(dat);
            queue.pop_front();
        }
        const double hz = sw.hz(num);
        zio::info("sync deque: {:.3f} MHz, {:.3f} GB/s", 1e-6 * hz,
                  1e-9 * siz * hz);
    }

    {
        zio::context_t ctx;
        auto socks = zio::create_linked_pairs(ctx);
        void *ptr = malloc(siz);

        zio::message_t msg(ptr, siz, nofree);  // dirty trick
        sw.restart();
        for (size_t ind = 0; ind < num; ++ind) {
            auto sres = socks.first.send(msg, zio::send_flags::none);
            assert(sres);

            zio::message_t msg2;
            auto rres = socks.second.recv(msg2, zio::recv_flags::none);
            assert(rres);
        }
        free(ptr);
        const double hz = sw.hz(num);
        zio::info("sync no free: {:.3f} MHz, {:.3f} GB/s", 1e-6 * hz,
                  1e-9 * siz * hz);
    }

    {
        zio::context_t ctx;
        auto socks = zio::create_linked_pairs(ctx);

        sw.restart();
        for (size_t ind = 0; ind < num; ++ind) {
            void *ptr = malloc(siz);
            zio::message_t msg(ptr, siz, dofree);  // dirty trick

            auto sres = socks.first.send(msg, zio::send_flags::none);
            assert(sres);

            zio::message_t msg2;
            auto rres = socks.second.recv(msg2, zio::recv_flags::none);
            assert(rres);
        }
        const double hz = sw.hz(num);
        zio::info("sync zero copy: {:.3f} MHz, {:.3f} GB/s", 1e-6 * hz,
                  1e-9 * siz * hz);
    }

    {
        zio::context_t ctx;
        auto socks = zio::create_linked_pairs(ctx);
        void *ptr = malloc(siz);
        zio::mutable_buffer mbuf = zio::buffer(ptr, siz);
        zio::const_buffer cbuf = zio::buffer((const void *)ptr, siz);

        sw.restart();
        for (size_t ind = 0; ind < num; ++ind) {
            auto sres = socks.first.send(cbuf, zio::send_flags::none);
            assert(sres);

            auto rres = socks.second.recv(mbuf, zio::recv_flags::none);
            assert(rres and !rres.value().truncated());
        }
        free(ptr);
        const double hz = sw.hz(num);
        zio::info("sync send/recv: {:.3f} MHz, {:.3f} GB/s", 1e-6 * hz,
                  1e-9 * siz * hz);
    }

    {
        zio::context_t ctx;
        const std::string addr = "inproc://asyncsendrecv";

        void *ptr = malloc(siz);
        zio::mutable_buffer mbuf = zio::buffer(ptr, siz);
        zio::const_buffer cbuf = zio::buffer((const void *)ptr, siz);

        sw.restart();
        auto t1 = std::thread([&]() {
            zio::socket_t src(ctx, zio::socket_type::push);
            src.bind(addr.c_str());
            for (size_t ind = 0; ind < num; ++ind) {
                auto sres = src.send(cbuf, zio::send_flags::none);
                assert(sres);
            }
        });
        auto t2 = std::thread([&]() {
            zio::socket_t dst(ctx, zio::socket_type::pull);
            dst.connect(addr.c_str());
            for (size_t ind = 0; ind < num; ++ind) {
                auto rres = dst.recv(mbuf, zio::recv_flags::none);
                assert(rres and !rres.value().truncated());
            }
        });
        t2.join();
        t1.join();

        free(ptr);
        const double hz = sw.hz(num);
        zio::info("async send/recv: {:.3f} MHz, {:.3f} GB/s", 1e-6 * hz,
                  1e-9 * siz * hz);
    }

    return 0;
}
