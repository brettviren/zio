#include "zio/tens.hpp"

#include <iostream>

int main()
{

    zio::cnpy::NpyArray tensor({2,3,4}, sizeof(float));
    std::cout << zio::tens::metaobj(tensor) << std::endl;

    zio::Message msg;
    zio::tens::init(msg);
    zio::tens::append(msg, tensor);
    std::cout << msg.prefix().dumps() << std::endl;
    
    zio::cnpy::NpyArray tensor2 = zio::tens::at(msg, 0);

    assert(tensor.word_size == tensor2.word_size);
    assert(tensor.num_vals == tensor2.num_vals);
    assert(tensor.shape.size() == tensor2.shape.size());
    const size_t ndims = tensor.shape.size();
    for (size_t ind=0; ind < ndims; ++ind) {
        assert(tensor.shape[ind] == tensor2.shape[ind]);
    }

    assert(tensor.num_bytes() == tensor2.num_bytes());
    const size_t nbytes = tensor.num_bytes();
    const char* t1 = tensor.data<char>();
    const char* t2 = tensor2.data<char>();
    for (size_t ind=0; ind<nbytes; ++ind) {
        assert(t1[ind] == t2[ind]);
    }

    return 0;
}
