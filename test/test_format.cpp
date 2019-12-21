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

template<typename CONVERTER>
struct testit_t {
    typedef CONVERTER converter_type;
    typedef typename converter_type::native_type native_type;
    native_type m_dat;
    converter_type convert;
    testit_t(native_type data) : m_dat(data) { }

    native_type native() { return m_dat; }
    zio::payload_t tobuffer(const native_type& nat) {
        zio::payload_t buf;
        bool ok = convert(m_dat, buf);
        assert(ok);
        return buf;
    }
    native_type tonative(const zio::payload_t& buf) {
        native_type nat;
        bool ok = convert(buf, nat);
        assert(ok);
        return nat;
    }

    void test() {
        assert (m_dat == native());
        assert (tonative(tobuffer(m_dat)) == m_dat);
    }
};

int main()
{
    testit_t<zio::converter::text_t> ttext(test_string);
    testit_t<zio::converter::json_t> tjson(test_json);
    testit_t<zio::converter::bson_t> tbson(test_json);
    testit_t<zio::converter::cbor_t> tcbor(test_json);
    testit_t<zio::converter::msgp_t> tmsgp(test_json);
    testit_t<zio::converter::ubjs_t> tubjs(test_json);

    return 0;
}
