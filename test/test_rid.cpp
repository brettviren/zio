#include <czmq.h>
#ifndef ZMQ_SERVER
#error 
#endif
int main()
{
    zsock_t* s = zsock_new(ZMQ_SERVER);
    zsock_bind(s, "inproc://testrid");
    zsock_t* c = zsock_new(ZMQ_CLIENT);
    zsock_connect(c, "inproc://testrid");

    zframe_t *f = zframe_new ("Hello", 5);
    assert(f);
    int rc = zframe_send(&f, c, 0);
    assert(rc == 0);
    assert(!f);

    f = zframe_recv(s);
    assert(f);
    assert (zframe_routing_id(f) != 0);
    //assert (zsock_routing_id(s) != 0);    

    zsock_destroy(&c);
    zsock_destroy(&s);
    return 0;
}
