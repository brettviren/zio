#include <czmq.h>


void test_owoa(bool backwards)
{
    zsock_t* c = zsock_new(ZMQ_CLIENT);
    assert(c);
    zsock_t* s = zsock_new(ZMQ_SERVER);
    assert(s);

    zsock_t* tobind=s, *toconn=c;
    if (backwards) {
        tobind=c;
        toconn=s;
    }
    const char * addr = "tcp://127.0.0.1:5678";
    int rc = 0;
    rc = zsock_bind(tobind, "%s", addr);
    if (rc < 0 ) {
        zsys_error("%d %s", errno, strerror(errno));
    }
    assert (rc >= 0);
    rc = zsock_connect(toconn, "%s", addr);
    assert(rc >= 0);

    int n = 42;
    rc = zsock_send(c,"i",n);
    assert(rc >= 0);
    rc = zsock_recv(s,"i",&n);
    assert(rc > 0);             // should be routing ID strictly >0
    assert(n==42);

    zsock_destroy(&c);
    zsock_destroy(&s);
}

int main()
{
    test_owoa(false);
    test_owoa(true);

    return 0;
}
