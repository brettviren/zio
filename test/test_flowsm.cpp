#include "zio/node.hpp"
#include "zio/util.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"
#include "zio/flow.hpp"
#include "sml.hpp"

#include <string>
#include <deque>
#include <queue>

// The main app holding mutable state operated on via actions.
struct FlowApp {
    zio::portptr_t port;

    int credit{0}, total_credit{0};
    int send_seqno{-1}, recv_seqno{-1};
    bool giver{true};
    std::string remid{""};

    std::string name() const {
        return port->name();
    }
};

namespace {

// Event types

struct SendMsg {
    zio::Message& msg;
};
struct RecvMsg {
    zio::Message& msg;
};

struct FlushPay {};
struct BeginFlow {};
struct CheckCredit {};


auto guard0 = [](auto e) {
                  zio::debug("guard1: {}", typeid(e).name());
                  return true; };
auto action1 = [](auto e, FlowApp& f) {
                   zio::debug("action1: {}, credit: {}",
                              typeid(e).name(), f.credit); ++f.credit; };

// template <class R, class... Ts>
// auto call_impl(R (*f)(Ts...)) {
//     return [f](Ts... args) { return f(args...); };
// }
// template <class T, class R, class... Ts>
// auto call_impl(T* self, R (T::*f)(Ts...)) {
//     return [self, f](Ts... args) { return (self->*f)(args...); };
// }
// template <class T, class R, class... Ts>
// auto call_impl(const T* self, R (T::*f)(Ts...) const) {
//     return [self, f](Ts... args) { return (self->*f)(args...); };
// }
// template <class T, class R, class... Ts>
// auto call_impl(const T* self, R (T::*f)(Ts...)) {
//     return [self, f](Ts... args) { return (self->*f)(args...); };
// }
// auto call = [](auto... args) { return call_impl(args...); };


// Guards

auto check_recv_bot = [](auto e, FlowApp& f) {
    if (f.recv_seqno != -1) {
        zio::debug("[flow {}] check_recv_bot recv_seqno={}",
                   f.name(), f.recv_seqno);
        return false;
    }
    
    zio::json fobj = e.msg.label_object();
    std::string typ = fobj["flow"].get<std::string>();
    if (typ != "BOT") {
        zio::debug("[flow {}] check_recv_bot called with '{}'",
                   f.name(), e.msg.label());
        return false;
    }
    zio::debug("[flow {}] check_recv_bot okay with '{}'",
              f.name(), e.msg.label());
    return true;
};                          

auto check_send_bot = [](auto e, FlowApp& f) { 
    if (f.send_seqno != -1) {
        zio::debug("[flow {}] check_send_bot send_seqno={}",
                   f.name(), f.send_seqno);
        return false;
    }
    
    zio::json fobj = e.msg.label_object();
    std::string typ = fobj["flow"].get<std::string>();
    if (typ != "BOT") {
        zio::debug("[flow {}] check_send_bot called with '{}'",
                   f.name(), e.msg.label());
        return false;
    }
    zio::debug("[flow {}] check_send_bot okay with '{}'",
              f.name(), e.msg.label());
    return true;
};

auto check_pay = [](auto e, FlowApp& f) {
    zio::json fobj = e.msg.label_object();
    std::string typ = fobj["flow"].get<std::string>();
    if (typ != "PAY") {
        zio::debug("[flow {}] check_pay not PAY '{}'",
                   f.name(), e.msg.label());
        return false;
    }
    if (! fobj["credit"].is_number()) {
        zio::debug("[flow {}] check_pay bad flow object '{}'",
                   f.name(), e.msg.label());
        return false;
    }
    int credit = fobj["credit"];
    if (credit + f.credit > f.total_credit) {
        zio::debug("[flow {}] check_pay too much PAY {} > {}",
                   f.name(), credit, f.total_credit);
        return false;
    }
    zio::debug("[flow {}] check_dat okay with '{}'",
              f.name(), e.msg.label());
    return true;
};

auto check_dat = [](auto e, FlowApp& f) {
    zio::json fobj = e.msg.label_object();
    std::string typ = fobj["flow"].get<std::string>();
    if (typ != "DAT") {
        zio::debug("[flow {}] check_dat not DAT '{}'",
                   f.name(), e.msg.label());
        return false;
    }
    zio::debug("[flow {}] check_dat okay with '{}'",
              f.name(), e.msg.label());
    return true;
};

auto check_eot = [](auto e, FlowApp& f) {
    zio::json fobj = e.msg.label_object();
    std::string typ = fobj["flow"].get<std::string>();
    if (typ != "EOT") {
        zio::debug("[flow {}] check_eot not EOT '{}'",
                   f.name(), e.msg.label());
        return false;
    }
    zio::debug("[flow {}] check_eot okay with '{}'",
              f.name(), e.msg.label());
    return true;
};

auto is_giver = [](auto e, FlowApp& f) { return f.giver; };

auto have_credit = [](auto e, FlowApp& f) {
    zio::debug("[flow {}] have_credit {}/{}",
               f.name(), f.credit, f.total_credit);
    return f.credit > 0;
};


// Actions


auto send_msg = [](auto e, FlowApp& f) {
    ++ f.send_seqno;
    e.msg.set_seqno(f.send_seqno);
    e.msg.set_remote_id(f.remid);
    f.port->send(e.msg);
    zio::debug("[flow {}] send_msg #{} with {}/{} credit",
               f.name(), f.send_seqno, f.credit, f.total_credit);
};

auto recv_bot = [](auto e, FlowApp& f) {
    zio::json fobj = e.msg.label_object();
    std::string dir = fobj["direction"];
    int credit = fobj["credit"];
    if (dir == "extract") { 
        f.giver = false;       // we are receiver
        f.total_credit = credit;
        f.credit = f.total_credit;
    }
    else if (dir == "inject") {
        f.giver = true;
        f.total_credit = credit;
        f.credit = 0;          
    }
    ++ f.recv_seqno;
    f.remid = e.msg.remote_id();
    zio::debug("[flow {}] recv_bot #{} as {} with {}/{} credit",
               f.name(), f.recv_seqno, dir, credit, f.total_credit);
};

auto recv_pay = [](auto e, FlowApp& f) {
    zio::json fobj = e.msg.label_object();
    int credit = fobj["credit"];
    f.credit += credit;
};

auto flush_pay = [](auto e, FlowApp& f) {
    if (!f.credit) {
        return;
    }
    zio::Message msg("FLOW");
    zio::json obj{{"flow","PAY"},{"credit",f.credit}};
    msg.set_label_object(obj);
    msg.set_seqno(++f.send_seqno);
    zio::debug("[flow {}] flush_pay #{}, credit:{}",
               f.port->name(), f.send_seqno, f.credit);
    f.credit=0;
    if (f.remid.size()) { msg.set_remote_id(f.remid); }
    f.port->send(msg);
};

auto recv_msg = [](auto e, FlowApp& f) {
    ++ f.recv_seqno;
};


// main states
struct CTOR{};
struct IDLE{};
struct BOTSEND{};
struct BOTRECV{};
struct READY{};
struct GIVING{};
struct FINACK{};
struct ACKFIN{};
struct FIN{};

// giving states
struct BROKE{};
struct GENEROUS{};
struct CREDITCHECK{};

// taking states
struct WALLETCHECK{};
struct HANDSOUT{};

struct flowsm_taking {
    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
* state<WALLETCHECK> + event<FlushPay> [have_credit] / flush_pay = state<HANDSOUT>
, state<HANDSOUT>    + event<RecvMsg>  [check_dat]  / (recv_msg, process(FlushPay{}))
            );
    }
};

struct flowsm_giving {
    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
            * state<BROKE>    + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            , state<GENEROUS> + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            , state<GENEROUS> + event<SendMsg> [check_dat] / (send_msg, process(CheckCredit{})) = state<CREDITCHECK>
            , state<CREDITCHECK> + event<CheckCredit> [ have_credit] = state<GENEROUS>
            , state<CREDITCHECK> + event<CheckCredit> [!have_credit] = state<BROKE>
            );
    }
};

struct flowsm {

    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
* state<CTOR> = state<IDLE>

, state<IDLE> + event<SendMsg> [check_send_bot] / send_msg = state<BOTSEND>
, state<IDLE> + event<RecvMsg> [check_recv_bot] / recv_bot = state<BOTRECV>

, state<BOTSEND> + event<RecvMsg> [check_recv_bot] / recv_bot = state<READY>
, state<BOTRECV> + event<SendMsg> [check_send_bot] / send_msg = state<READY>

, state<READY> + event<BeginFlow> [ is_giver ]             = state<flowsm_giving>
, state<READY> + event<BeginFlow> [!is_giver ] / (process(FlushPay{})) = state<flowsm_taking>

, state<flowsm_giving> + event<SendMsg> [check_eot] / send_msg = state<ACKFIN>
, state<flowsm_giving> + event<RecvMsg> [check_eot] / recv_msg = state<FINACK>
, state<flowsm_taking> + event<SendMsg> [check_eot] / send_msg = state<ACKFIN>
, state<flowsm_taking> + event<RecvMsg> [check_eot] / recv_msg = state<FINACK>

, state<FINACK> + event<SendMsg> [check_eot] / send_msg = state<FIN>
, state<ACKFIN> + event<RecvMsg> [check_eot] / recv_msg = state<FIN>

            );
    }
};

} // generic namespace


int main()
{
    using namespace boost;

    zio::init_all();

    // this test interleaves client and server conversation

    zio::Node snode("server", 1);
    auto sport = snode.port("recver", ZMQ_SERVER);
    sport->bind();
    snode.online();
    zio::Node cnode("client", 2);
    auto cport = cnode.port("sender", ZMQ_CLIENT);
    cport->connect("server","recver");
    cnode.online();

    typedef sml::sm<flowsm, sml::defer_queue<std::deque>, sml::process_queue<std::queue>> FlowSM;

    FlowApp sflow{sport};
    FlowApp cflow{cport};

    FlowSM sm{sflow};
    assert(sm.is(sml::state<IDLE>));
    assert(sflow.send_seqno == -1);
    FlowSM cm{cflow};
    assert(cm.is(sml::state<IDLE>));
    assert(cflow.send_seqno == -1);

    // CLIENT: initial BOT
    {
        zio::Message cbot("FLOW");
        cbot.set_label_object({{"flow","BOT"},
                               {"direction","extract"},
                               {"credit",2}});

        auto ok = cm.process_event(SendMsg{cbot});
        assert(ok);
        assert(cm.is(sml::state<BOTSEND>));
        assert(cflow.send_seqno == 0);
    }
    
    // CLIENT: try to send again.  This apparently quietly does
    // nothing.  Is there a way to get some error?
    {
        zio::debug("client: try double send");

        zio::Message cbot("FLOW");
        cbot.set_label_object({{"flow","BOT"},
                               {"direction","extract"},
                               {"credit",2}});

        auto ok = cm.process_event(SendMsg{cbot});
        assert(!ok);
        assert(cm.is(sml::state<BOTSEND>));
        assert(cflow.send_seqno == 0);
    }

    // SERVER: do a recv, better be a BOT
    {
        zio::Message sbot("FLOW");
        assert(sport->recv(sbot));

        {
            auto ok = sm.process_event(RecvMsg{sbot});
            assert(ok);
            assert(sm.is(sml::state<BOTRECV>));
            assert(sflow.recv_seqno == 0);
        }
        {
            zio::debug("server: try double recv");
            auto ok = sm.process_event(RecvMsg{sbot});
            assert(!ok);
            assert(sm.is(sml::state<BOTRECV>));
            assert(sflow.recv_seqno == 0);
        }

        // SERVER responds to BOT, reuse sbot
        auto fobj = sbot.label_object();
        zio::debug("server bot: {}", fobj.dump());
        assert(fobj["direction"].get<std::string>() == "extract");
        fobj["direction"] = "inject"; // server
        sbot.set_label_object(fobj);

        auto ok = sm.process_event(SendMsg{sbot});
        assert(ok);
        assert(sm.is(sml::state<READY>));
        assert(sflow.send_seqno == 0);
        assert(!sflow.giver);
    }

    // CLIENT do a recv, better be a bot
    {
        zio::Message cbot;
        assert(cport->recv(cbot));

        auto ok = cm.process_event(RecvMsg{cbot});
        assert(ok);
        assert(cm.is(sml::state<READY>));
        assert(cflow.recv_seqno == 0);
        assert(cflow.giver);
    }

    // Explicitly leave READY state
    {
        auto ok = sm.process_event(BeginFlow{});
        assert(ok);
        assert(sm.is< decltype(sml::state<flowsm_taking>) >(sml::state<HANDSOUT>));
    }

    {
        auto ok = cm.process_event(BeginFlow{});
        assert(ok);
        assert(cm.is< decltype(sml::state<flowsm_giving>) >(sml::state<BROKE>));
    }


    // both are now in flow
    
    {
        zio::Message cpay;
        assert(cport->recv(cpay));
        auto ok = cm.process_event(RecvMsg{cpay});
        assert(ok);
        assert(cm.is< decltype(sml::state<flowsm_giving>) >(sml::state<GENEROUS>));
    }

    {
        zio::Message cdat("FLOW");
        cdat.set_label_object({{"flow","DAT"}});

        auto ok = cm.process_event(SendMsg{cdat});
        assert(ok);
        assert(cm.is< decltype(sml::state<flowsm_giving>) >(sml::state<GENEROUS>));
    }

    {
        zio::Message sdat;
        assert(sport->recv(sdat));
        auto ok = sm.process_event(RecvMsg{sdat});
        assert(ok);
        assert(sm.is< decltype(sml::state<flowsm_taking>) >(sml::state<HANDSOUT>));
    }

    // shut it down

    {                           // client
        zio::Message ceot("FLOW");
        ceot.set_label_object({{"flow","EOT"}});
        auto ok = cm.process_event(SendMsg{ceot});
        assert(ok);
        assert(cm.is(sml::state<ACKFIN>));
    }

    {                           // server
        zio::Message seot;
        assert(sport->recv(seot));
        {
            auto ok = sm.process_event(RecvMsg{seot});
            assert(ok);
            assert(sm.is(sml::state<FINACK>));
        }
        {
            auto ok = sm.process_event(SendMsg{seot});
            assert(ok);
            assert(sm.is(sml::state<FIN>));
        }
    }

    {                           // client
        zio::Message ceot;
        assert(cport->recv(ceot));
        auto ok = cm.process_event(RecvMsg{ceot});
        assert (ok);
        assert(cm.is(sml::state<FIN>));
    }    

    return 0;
}
