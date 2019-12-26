#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <functional>
#include <stdexcept>
#include "sml.hpp"

struct flow_protocol {

    typedef int endpoint_t;
    enum FlowDirection { undefined, extract, inject };
    typedef int direction_t;
    typedef int credits_t;

    // Events
    struct ev_error {};

    // All events which are also message have common base
    struct ev_msg {
        endpoint_t ep; // 0 means message came from a server thus we are in client mode
    };
    // begin-of-transmission
    struct ev_bot : public ev_msg {
        FlowDirection fdir; // server&&extract or client&&inject == recving
        credits_t credits;
    };
    // end-of-tranmission
    struct ev_eot : public ev_msg {
        bool ack;               // if true, send EOT to other end
    };
    typedef ev_bot ev_checksr;
    struct ev_sending{
        int credits;
    };
    struct ev_recving{
        int credits;
    };
    struct ev_dat : public ev_msg {
        int data;
    };
    struct ev_pay : public ev_msg {
        int credits;
    };

    // Injected event callbacks
    struct eventcbs {
        std::function<void()> error;
    };
    // Injected action callbacks
    struct actioncbs {
        std::function<void(bool initiator)> disconnect;
        // use for server, BOT comes from app, should send a BOT to client
        std::function<void(const ev_bot&)> send_bot;
        // use for client, BOT comes from server
        std::function<void(const ev_bot&)> recv_bot;
        // notify and maybe acknowoledge an EOT
        std::function<void(const ev_eot&)> eot;
        std::function<void(const ev_dat&)> got_dat;
        std::function<void(const ev_pay&)> got_pay;
    };


    struct protocol {
        int credits{0};
        int total_credits{0};

        auto operator()() {
            namespace sml = boost::sml;
            using namespace sml;

            auto we_are_server = [](const ev_bot& b) { return b.ep>0; };
            auto we_are_client = [](const ev_bot& b) { return b.ep==0; };

            return make_transition_table(

                *"establishing"_s + sml::event<ev_bot> [we_are_client] / (
                    [this](const ev_bot& ev, actioncbs& ac) {
                        total_credits = ev.credits;
                        ac.recv_bot(ev);
                    },
                    [this](const ev_bot& ev, back::process<ev_sending, ev_recving> p) {
                        if (ev.fdir == inject) {
                            p(ev_recving{ev.credits});
                        }
                        else {
                            p(ev_sending{ev.credits});
                        }
                    }) = "established"_s

                ,"establishing"_s + sml::event<ev_bot> [we_are_server] / (
                    [this](const ev_bot& ev, actioncbs& ac) {
                        total_credits = ev.credits;
                        ac.recv_bot(ev);
                    },
                    [this](const ev_bot& ev, back::process<ev_sending, ev_recving> p) {
                        if (ev.fdir == extract) {
                            p(ev_recving{ev.credits});
                        }
                        else {
                            p(ev_sending{ev.credits});
                        }
                    }) = "established"_s

                ,"established"_s + on_entry<_> / []{std::cout<<"established\n";}
                ,"established"_s + event<ev_sending> / [this](const ev_sending& ev){credits=ev.credits;} = "sending"_s
                ,"established"_s + event<ev_recving> / [this](const ev_recving& ev){credits=0;} = "recving"_s


                ,"sending"_s + on_entry<_> / []{std::cout<<"wait for pay\n";}
                ,"sending"_s + event<ev_pay> / [this](const ev_pay& p, actioncbs&ac) {
                                                   credits += p.credits;
                                                   ac.got_pay(p);
                                               }


                ,"recving"_s + on_entry<_> / []{std::cout<<"wait for dat\n";}
                ,"recving"_s + event<ev_dat> / [this](const ev_dat& d, actioncbs&ac) {
                                                   ++credits;
                                                   ac.got_dat(d);
                                               }


                ,*"bad state"_s + unexpected_event<_> / [](eventcbs& ev){ev.error();} = X
                );
        }
    };

    auto operator()() {
        namespace sml = boost::sml;
        using namespace sml;

        return make_transition_table(
            *state<protocol> + event<ev_eot> / 
            [](const ev_eot& ev, actioncbs& ac)
                {
                    ac.eot(ev);
                } = "fin"_s
            ,"fin"_s / []{std::cout<<"exiting\n";} = X

            );
    }
    

};

std::ostream& operator<<(std::ostream& o, const flow_protocol::ev_bot& bot)
{
    o << "<BOT ep=" << bot.ep << " fdir=" << bot.fdir << " credits=" << bot.credits << ">";
    return o;
}
std::ostream& operator<<(std::ostream& o, const flow_protocol::ev_eot& eot)
{
    o << "<EOT ep=" << eot.ep << " ack=" << eot.ack << ">";
    return o;
}
std::ostream& operator<<(std::ostream& o, const flow_protocol::ev_dat& dat)
{
    o << "<DAT ep=" << dat.ep << " data=" << dat.data << ">";
    return o;
}
std::ostream& operator<<(std::ostream& o, const flow_protocol::ev_pay& pay)
{
    o << "<PAY ep=" << pay.ep << " credits=" << pay.credits << ">";
    return o;
}

#include <deque>
#include <queue>

int main()
{
    namespace sml = boost::sml;
    using namespace sml;

    flow_protocol::actioncbs ac {
        [](bool initiator) { std::cout << "disconnect "<<initiator<<"\n"; },
        [](const flow_protocol::ev_bot& bot) { std::cout << "send_bot(" << bot << ")\n"; },
        [](const flow_protocol::ev_bot& bot) { std::cout << "recv_bot(" << bot << ")\n"; },
        [](const flow_protocol::ev_eot& eot) { std::cout << "eot(" << eot << ")\n"; },
        [](const flow_protocol::ev_dat& dat) { std::cout << "got_dat(" << dat << ")\n"; },
        [](const flow_protocol::ev_pay& pay) { std::cout << "got_pay(" << pay << ")\n"; }
    };
    flow_protocol::eventcbs ev {
        []() { std::cout << "error\n"; },
    };

    sm<flow_protocol, sml::defer_queue<std::deque>, sml::process_queue<std::queue> > mysm{ac, ev};
    assert(mysm.is(state<flow_protocol::protocol>));
    // non-zero ID means message came from a client so SM is a server.
    // server+extract means SM is recving
    mysm.process_event(flow_protocol::ev_bot{1,flow_protocol::extract, 3});
    assert(mysm.is(state<flow_protocol::protocol>));
    //assert(mysm.is<decltype(state<flow_protocol::protocol>)>("established"_s));
    //assert(mysm.is<decltype(state<flow_protocol::protocol>)>(X));
    //assert(!mysm.is(X));

    mysm.process_event(flow_protocol::ev_dat{1,100});
    mysm.process_event(flow_protocol::ev_dat{0,100}); // changing ID is an internal error, SM won't catch

    mysm.process_event(flow_protocol::ev_eot{1});
    assert(mysm.is(X));
};

