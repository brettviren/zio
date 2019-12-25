// example from readme
#include <iostream>
#include <cassert>
#include "sml.hpp"
namespace sml = boost::sml;

// Dependencies
struct sender {
    template<class TMsg>
    constexpr void send(const TMsg& msg) { std::cout << "send: "<< msg.id << std::endl; }
};

// Events

struct ack { bool valid{}; };
struct fin { int id{}; bool valid{}; };
struct release {};
struct timeout {};

// Guards

constexpr auto is_valid = [](const auto& event) { return event.valid; };

// Actions

constexpr auto send_fin = [](sender& s) { s.send(fin{0}); };
constexpr auto send_ack = [](const auto& event, sender& s) { s.send(event); };

// State Machine

struct tcp_release /*final*/ {
    auto operator()() const {
        using namespace sml;
        /**
         * Initial state: *initial_state
         * Transition DSL: src_state + event [ guard ] / action = dst_state
         */
        return make_transition_table(
            *"established"_s + event<release>          / send_fin  = "fin wait 1"_s,
            "fin wait 1"_s  + event<ack> [ is_valid ]             = "fin wait 2"_s,
            "fin wait 2"_s  + event<fin> [ is_valid ] / send_ack  = "timed wait"_s,
            "timed wait"_s  + event<timeout>                      = X
            );
    }
};

// Usage

int main() {
    using namespace sml;

    sender s{};
    sm<tcp_release> mysm{s}; // pass dependencies via ctor
    assert(mysm.is("established"_s));

    mysm.process_event(release{}); // complexity O(1)
    assert(mysm.is("fin wait 1"_s));

    mysm.process_event(ack{true}); // prints 'send: 0'
    assert(mysm.is("fin wait 2"_s));

    mysm.process_event(fin{42, true}); // prints 'send: 42'
    assert(mysm.is("timed wait"_s));

    mysm.process_event(timeout{});
    assert(mysm.is(X));  // terminated
}
