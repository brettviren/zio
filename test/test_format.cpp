#include "zio/format.hpp"

#include <iostream>
using namespace std;

#define test_string "Hello World!"
#define test_json {{"msg","Hello World!"},{"bye","Cruel world..."}}

/*
BUFF
TEXT
JSON
BSON
CBOR
MSGP
UBJS
*/

template<typename TYPE>
TYPE makeit_string()
{
    std::string str = test_string;
    return TYPE(str);
}
template<typename TYPE>
TYPE makeit_datasize()
{
    const char* data = test_string;
    return TYPE(data,strlen(data));
}
template<typename TYPE>
TYPE makeit_cstr()
{
    return TYPE(test_string);
}

template<typename TYPE>
void assert_equal_str(TYPE obj)
{
    std::string want = test_string;
    std::string got = obj();
    //cerr << "want: '" << want << "', got: '" << got << "'\n";
    assert(want == got);
}
template<typename TYPE>
void testit_str()
{
    assert_equal_str<TYPE>(makeit_cstr<TYPE>());
    assert_equal_str<TYPE>(makeit_datasize<TYPE>());
    assert_equal_str<TYPE>(makeit_string<TYPE>());
}

template<typename TYPE>
TYPE makeit_json()
{
    nlohmann::json j = test_json;
    return TYPE (j);
}

template<typename TYPE>
TYPE callit_json(const nlohmann::json& j)
{
    return TYPE (j);
}

template<typename TYPE>
void assert_equal_json(TYPE obj)
{
    nlohmann::json want = test_json;
    nlohmann::json got = obj();
    cerr << "want:\n" << want << "\ngot:\n" << got << "\n";
    assert(want == got);
}

template<typename TYPE>
void testit_json()
{
    assert_equal_json<TYPE>(makeit_json<TYPE>());
    assert_equal_json<TYPE>(callit_json<TYPE>(test_json));
}

int main()
{
    testit_str<zio::BUFF>();
    testit_str<zio::TEXT>();

    testit_json<zio::JSON>();
    testit_json<zio::BSON>();
    testit_json<zio::CBOR>();
    testit_json<zio::MSGP>();
    testit_json<zio::UBJS>();

    return 0;
}
