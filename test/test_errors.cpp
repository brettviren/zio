#include "zio/interned.hpp"

#include <iostream>

static
void test_socket()
{
    zio::context_t ctx;
    zio::socket_t s(ctx, ZMQ_PUB);
    try {
        s.bind("wrong");
    }
    catch (zio::error_t& se) {
        std::cout << "Correctly caught " << se.what() << std::endl;
    }
    try {
        s.connect("wrong");
    }
    catch (zio::error_t& se) {
        std::cout << "Correctly caught " << se.what() << std::endl;
    }
    
}

int main()
{
    test_socket();
}
