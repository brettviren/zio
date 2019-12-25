#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include "sml.hpp"
namespace sml = boost::sml;

// Events

struct BOT { std::vector<std::string> streams; int credits; };
struct EOT {};
struct BOS { std::string stream; };
struct EOS { std::string stream; };
struct DAT { std::string stream; };

// Dependencies
struct server {

    template<class TMsg>
    constexpr void send(const TMsg& msg) { std::cout << "send: "<< msg.id << std::endl; }

    bool have_stream(const std::string& stream) {
        std::cout << "have_stream("<< stream << ")\n";
        return true;
    }

    template<class TMsg>
    void accept_message(const TMsg& msg) {
        std::cout << "got: "<< msg.stream << std::endl;
    }

    void send_pay() {
        std::cout << "sending pay\n";
    }
};

template<>
void server::accept_message<BOT>(const BOT& bot) {
    std::cout << "got: BOT: "<< bot.credits << std::endl;
}
template<>
void server::accept_message<EOT>(const EOT& ) {
    std::cout << "got: EOT" << std::endl;
}


// Guard

constexpr auto known_stream = [](const auto& event, server& server) {
                                  return server.have_stream(event.stream);
                              };

// Actions

constexpr auto accept_msg = [](const auto& event, server& s) {
                                s.accept_message(event);
                            };
constexpr auto send_pay = [](server& s) {
                              s.send_pay();
                          };

struct extract_server /*final*/ {
    auto operator()() const {
        using namespace sml;
        /**
         * Initial state: *initial_state
         * Transition DSL: src_state + event [ guard ] / action = dst_state
         */
        return make_transition_table(
            *"established"_s + event<BOT>          / accept_msg  = "transmitting"_s,
            "transmitting"_s  + event<BOS> [ known_stream ] / (accept_msg,send_pay) = "ready"_s,
            "ready"_s + event<BOS> [ known_stream ] / (accept_msg,send_pay)  = "ready"_s,
            "ready"_s + event<EOS> [ known_stream ] / (accept_msg,send_pay)  = "ready"_s,
            "ready"_s + event<DAT> [ known_stream ] / (accept_msg,send_pay)  = "ready"_s,
            "transmitting"_s + event<EOT> / (accept_msg,send_pay) = "established"_s
            );
    }
};

int main() {
    using namespace sml;

    server s{};
    sm<extract_server> mysm{s};
    assert(mysm.is("established"_s));

    mysm.process_event(BOT{{"stream1","stream2"}, 2});
    assert(mysm.is("transmitting"_s));

    mysm.process_event(BOS{"stream1"});
    assert(mysm.is("ready"_s));

    mysm.process_event(EOS{"stream1"});
    assert(mysm.is("ready"_s));

    mysm.process_event(EOT{});
    assert(mysm.is("established"_s));

    // mysm.process_event(timeout{});
    // assert(mysm.is(X));  // terminated

    return 0;
}
