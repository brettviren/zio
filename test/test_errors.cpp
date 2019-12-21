#include "zio/socket.hpp"
#include "zio/exceptions.hpp"

#include <iostream>

static
void test_socket()
{
    zio::Socket s(ZMQ_PUB);
    try {
        s.bind("wrong");
    }
    catch (zio::socket_error& se) {
        std::cout << "Correctly caught " << se.what() << std::endl;
    }
    try {
        s.connect("wrong");
    }
    catch (zio::socket_error& se) {
        std::cout << "Correctly caught " << se.what() << std::endl;
    }
    
}

int main()
{
    test_socket();
}
