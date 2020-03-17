// zio::domo needs a way to queue multipart messages.

#include "zio/zmq_addon.hpp"
#include <deque>
#include <cassert>

void take_away(zmq::multipart_t& mmsg)
{
    std::deque<zmq::multipart_t> q;
    q.emplace_back(std::move(mmsg));
    assert(mmsg.size() == 0);
    assert(q.size() == 1);
    assert(q[0].size() == 1);
}


int main()
{
    {
        std::deque<zmq::multipart_t> q;
        zmq::multipart_t mp;
        q.emplace_back(std::move(mp));
        zmq::multipart_t mp2(std::move(q.front()));
        q.pop_front();
        assert(q.size() == 0);
    }

    {
        zmq::multipart_t mp;
        mp.pushmem(NULL,0);
        take_away(mp);
        assert(mp.size() == 0);
    }

    return 0;
}
