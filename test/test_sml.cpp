// example from readme
#include <iostream>
#include <cassert>
#include "zio/sml.hpp"

namespace sml = boost::sml;

// Dependencies
struct sender
{
    template <class TMsg>
    constexpr void send(const TMsg& msg)
    {
        std::cout << "send: " << msg.id << std::endl;
    }
};

// Events

namespace foo {
struct ack
{
    bool valid{};
};
struct fin
{
    int id{};
    bool valid{};
};
struct release
{
};
struct timeout
{
};


// Guards

constexpr auto is_valid = [](const auto& event) { return event.valid; };

// Actions

constexpr auto send_fin = [](sender& s) { s.send(foo::fin{0}); };
constexpr auto send_ack = [](const auto& event, sender& s) { s.send(event); };

} // namespace foo

// State Machine

struct tcp_release /*final*/
{
    auto operator()() const
    {
        using namespace sml;
        /**
         * Initial state: *initial_state
         * Transition DSL: src_state + event [ guard ] / action = dst_state
         */
        // clang-format off
        return make_transition_table(
            *"established"_s + event<foo::release> / foo::send_fin = "fin wait 1"_s,
            "fin wait 1"_s + event<foo::ack>[foo::is_valid] = "fin wait 2"_s,
            "fin wait 2"_s + event<foo::fin>[foo::is_valid] / foo::send_ack = "timed wait"_s,
            "timed wait"_s + event<foo::timeout> = X);
        // clang-format on
    }
};

// Usage

int main()
{
    using namespace sml;

    sender s{};
    sm<tcp_release> mysm{s};  // pass dependencies via ctor
    assert(mysm.is("established"_s));

    mysm.process_event(foo::release{});  // complexity O(1)
    assert(mysm.is("fin wait 1"_s));

    mysm.process_event(foo::ack{true});  // prints 'send: 0'
    assert(mysm.is("fin wait 2"_s));

    mysm.process_event(foo::fin{42, true});  // prints 'send: 42'
    assert(mysm.is("timed wait"_s));

    mysm.process_event(foo::timeout{});
    assert(mysm.is(X));  // terminated
}
