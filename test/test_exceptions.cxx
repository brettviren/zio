#include "zio/exceptions.hpp"

#include <iostream>

int main()
{
    try {
        throw zio::socket_error::create(42, "question unknown", "extra smack talk");
    }
    catch (zio::exception& e) {
        std::cout << e.what() << std::endl;
    }

    auto e = zio::socket_error::create();
    std::cout << e.what() << std::endl;    
    auto m = zio::message_error::create(404, "communication breakdown");
    std::cout << m.what() << std::endl;
    return 0;
}
