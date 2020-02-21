#include "zio/tens.hpp"
#include <complex>
#include <typeinfo>

// #include <iostream>

// stolen from cnpy

const char* zio::tens::type_name(const std::type_info& t)
{
    // std::cerr << t.name() << " " << typeid(float).name() << std::endl;

    if(t == typeid(float) ) return "f";
    if(t == typeid(double) ) return "f";
    if(t == typeid(long double) ) return "f";

    if(t == typeid(int) ) return "i";
    if(t == typeid(char) ) return "i";
    if(t == typeid(short) ) return "i";
    if(t == typeid(long) ) return "i";
    if(t == typeid(long long) ) return "i";

    if(t == typeid(unsigned char) ) return "u";
    if(t == typeid(unsigned short) ) return "u";
    if(t == typeid(unsigned long) ) return "u";
    if(t == typeid(unsigned long long) ) return "u";
    if(t == typeid(unsigned int) ) return "u";

    if(t == typeid(bool) ) return "b";

    if(t == typeid(std::complex<float>) ) return "c";
    if(t == typeid(std::complex<double>) ) return "c";
    if(t == typeid(std::complex<long double>) ) return "c";

    else return "?";
}


void zio::tens::append(zio::Message& msg, zio::message_t&& data,
                       const std::vector<size_t>& shape,
                       size_t word_size, const char* tn)
{
    msg.set_form(zio::tens::form);
    zio::json lobj = zio::json::value_t::object;
    std::string label = msg.label();
    if (! label.empty()) {
        lobj = zio::json::parse(label);
    }
    zio::json md = {
        {"shape", shape},
        {"word", word_size},
        {"dtype", tn},
        {"part", msg.payload().size()}
        // no order as this is C++
    };
    lobj[zio::tens::form].push_back(md);
    msg.set_label_object(lobj);
    msg.add(std::move(data));
}

const zio::message_t& zio::tens::at(const Message& msg, size_t index)
{
    static const zio::message_t bogus;

    auto lobj = msg.label_object();
    auto ta = lobj[zio::tens::form];
    auto md = ta[index];

    if (md.is_null()) {
        return bogus;
    }
    size_t part = index;
    if (md["part"].is_number()) {
        part = md["part"].get<size_t>();
    }
    if (part < 0 or part >= msg.payload().size()) {
        return bogus;
    }
    return msg.payload()[part];
}
