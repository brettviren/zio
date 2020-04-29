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

    struct established {};
    struct fin_wait_1 {};
    struct fin_wait_2 {};
    struct timed_wait {};

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
            *state<foo::established> + event<foo::release> / foo::send_fin = state<foo::fin_wait_1>,
            state<foo::fin_wait_1> + event<foo::ack>[foo::is_valid] = state<foo::fin_wait_2>,
            state<foo::fin_wait_2> + event<foo::fin>[foo::is_valid] / foo::send_ack = state<foo::timed_wait>,
            state<foo::timed_wait> + event<foo::timeout> = X);
        // clang-format on
    }
};

// Usage

int main()
{
    using namespace sml;

    sender s{};
    sm<tcp_release> mysm{s};  // pass dependencies via ctor
    assert(mysm.is(state<foo::established>));

    mysm.process_event(foo::release{});  // complexity O(1)
    assert(mysm.is(state<foo::fin_wait_1>));

    mysm.process_event(foo::ack{true});  // prints 'send: 0'
    assert(mysm.is(state<foo::fin_wait_2>));

    mysm.process_event(foo::fin{42, true});  // prints 'send: 42'
    assert(mysm.is(state<foo::timed_wait>));

    mysm.process_event(foo::timeout{});
    assert(mysm.is(X));  // terminated
}
