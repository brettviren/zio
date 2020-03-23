#include "zio/util.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"

int main()
{
    zio::init_all();
    uint32_t rid1 = 0xfeefff01;

    auto remid = zio::to_remid(rid1);
    auto rid2 = zio::to_rid(remid);
    zio::debug("rid1={:x} rid2={:x} remid={}", rid1, rid2, zio::binstr(remid));

    assert(rid1 == rid2);

    return 0;
}
