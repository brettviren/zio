#include "zio/tens.hpp"

zio::json zio::tens::metaobj(const cnpy::NpyArray& tensor)
{
    return zio::json({{"shape", tensor.shape}, {"word", tensor.word_size}});
}

void zio::tens::init(zio::Message& msg)
{
    msg.set_form(zio::tens::form);
    msg.set_label_object({{zio::tens::form, json::value_t::array}});
}

void zio::tens::append(zio::Message& msg, const zio::cnpy::NpyArray& tensor)
{
    auto lobj = msg.label_object();
    auto& ta = lobj[zio::tens::form];
    ta.push_back(metaobj(tensor));
    msg.set_label_object(lobj);
    msg.add(zio::message_t(tensor.data<std::byte>(), tensor.num_bytes()));
}

zio::cnpy::NpyArray zio::tens::at(const zio::Message& msg, size_t index)
{
    if (msg.form() != zio::tens::form) {
        return zio::cnpy::NpyArray();
    }
    auto lobj = msg.label_object();
    auto ta = lobj[zio::tens::form];
    auto md = ta[index];

    std::vector<size_t> shape = md["shape"].get< std::vector<size_t> >();
    size_t word = md["word"].get<size_t>();
    
    zio::cnpy::NpyArray ret(shape, word);
    const auto& spmsg = msg.payload()[index];
    assert(spmsg.size() == ret.num_bytes());
    memcpy(ret.data<char>(), spmsg.data(), ret.num_bytes());
    return ret;
}

