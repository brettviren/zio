// zio::domo needs a way to queue multipart messages.

#include "zio/cppzmq.hpp"
#include <deque>
#include <cassert>

void take_away(zio::multipart_t& mmsg)
{
    std::deque<zio::multipart_t> q;
    q.emplace_back(std::move(mmsg));
    assert(mmsg.size() == 0);
    assert(q.size() == 1);
    assert(q[0].size() == 1);
}

int main()
{
    {
        std::deque<zio::multipart_t> q;
        zio::multipart_t mp;
        q.emplace_back(std::move(mp));
        zio::multipart_t mp2(std::move(q.front()));
        q.pop_front();
        assert(q.size() == 0);
    }

    {
        zio::multipart_t mp;
        mp.pushmem(NULL, 0);
        take_away(mp);
        assert(mp.size() == 0);
    }

    return 0;
}
