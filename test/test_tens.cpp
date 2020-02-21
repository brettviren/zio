#include "zio/tens.hpp"

#include <iostream>

int main()
{

    float tensor[2][3][4] = {0};
    const float* tensor1 = (float*) tensor;

    std::vector<size_t> shape={2,3,4};

    zio::Message msg(zio::tens::form);

    // Add an initial, unrelated message part just to make sure tens
    // respects it.
    msg.add(zio::message_t((char*)nullptr,0));

    zio::tens::append(msg, tensor1, shape);
    assert(msg.payload().size() == 2);

    assert(msg.form() == zio::tens::form);
    auto lobj = msg.label_object();
    std::cout << lobj << std::endl;
    auto md = lobj[zio::tens::form][0];
    
    for (int ind=0; ind < 3; ++ind) {
        assert(shape[ind] == md["shape"][ind].get<size_t>());
    }

    const float* tensor2 = zio::tens::at<float>(msg, 0);
    assert(tensor2);
    for (size_t ind=0; ind<24; ++ind) {
        std::cout<<ind<<" " << tensor1[ind] << " " <<  tensor2[ind] << std::endl;
        assert(tensor1[ind] == tensor2[ind]);
    }

    const zio::message_t& spmp = zio::tens::at(msg, 0);
    assert(spmp.size());
    assert(!spmp.empty());

    return 0;
}
