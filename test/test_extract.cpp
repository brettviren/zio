#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include "sml.hpp"
namespace sml = boost::sml;

// Events

typedef std::string client_t;

struct BOT { client_t client; int credits; };
struct EOT { client_t client; };
struct DAT { client_t client; };
struct PAY { client_t client; };

// Dependencies
struct server {

    template<class TMsg>
    constexpr void send(const TMsg& msg) { std::cout << "send to: "<< msg.client << std::endl; }

    bool have_client(const client_t& client) {
        std::cout << "have_client("<< client << ")\n";
        return true;
    }

    template<class TMsg>
    void accept_message(const TMsg& msg) {
        std::cout << "got: "<< msg.client << std::endl;
    }

    template<class TMsg>
    void send_bot(const TMsg& bot) {
        std::cout << "sending bot to " << bot.client << "\n";
    }
    template<class TMsg>
    void send_eot(const TMsg& eot) {
        std::cout << "sending eot to " << eot.client << "\n";
    }
    template<class TMsg>
    void send_pay(const TMsg& dat) {
        std::cout << "sending pay to " << dat.client << "\n";
    }
    template<class TMsg>
    void send_dat(const TMsg& pay) {
        std::cout << "sending dat to " << pay.client << "\n";
    }

    template<class TMsg>
    void term_client(const TMsg& msg) {
        std::cout << "terminating client " << msg.client << "\n";
    }


};

template<>
void server::accept_message<BOT>(const BOT& bot) {
    std::cout << "got: "<<bot.client<<" BOT: "<< bot.credits << std::endl;
}
template<>
void server::accept_message<EOT>(const EOT& eot) {
    std::cout << "got: "<<eot.client<<" EOT" << std::endl;
}
template<>
void server::accept_message<DAT>(const DAT& dat) {
    std::cout << "got: "<<dat.client<<" DAT" << std::endl;
}


// Guard

constexpr auto known_client = [](const auto& event, server& server) {
                                  return server.have_client(event.client);
                              };

// Actions

constexpr auto accept_dat = [](const auto& event, server& s) {
                                s.accept_message(event);
                            };
constexpr auto accept_pay = [](const auto& event, server& s) {
                                s.accept_message(event);
                            };

constexpr auto send_bot = [](const BOT& event, server& s) {
                              s.send_bot(event);
                          };
constexpr auto send_eot = [](const auto& event, server& s) {
                              s.send_eot(event);
                          };
constexpr auto send_dat = [](const auto& event, server& s) {
                              s.send_dat(event);
                          };
constexpr auto send_pay = [](const auto& event, server& s) {
                              s.send_pay(event);
                          };
constexpr auto term_client = [](const auto& event, server& s) {
                              s.term_client(event);
                          };

struct transmitting {
    auto operator()() const {
        using namespace sml;
        return make_transition_table(
            *"begin"_s  + on_entry<_> / send_bot = "waiting"_s,
            "waiting"_s + event<DAT> / (accept_dat, send_pay) = "waiting"_s,
            "waiting"_s + event<PAY> / (accept_pay, send_dat) = "waiting"_s
            );
    }
            
};

struct extract_server /*final*/ {
    auto operator()() const {
        using namespace sml;
        /**
         * Initial state: *initial_state
         * Transition DSL: src_state + event [ guard ] / action = dst_state
         */
        return make_transition_table(
            *"established"_s + event<BOT> [ known_client ] / term_client = state<transmitting>,
            *"established"_s + event<BOT> [ !known_client ] = state<transmitting>,
            state<transmitting> + event<EOT> / (term_client,send_eot) = "established"_s
            );
    }
};

int main() {
    using namespace sml;

    server s{};
    sm<extract_server> mysm{s};
    assert(mysm.is("established"_s));

//    mysm.process_event(BOT{"client1", 2});
//    assert(mysm.is(state<transmitting>));

//    mysm.process_event(EOT{"client1"});
//    assert(mysm.is("established"_s));

    // mysm.process_event(timeout{});
    // assert(mysm.is(X));  // terminated

    return 0;
}
