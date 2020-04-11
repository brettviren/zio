#include "zio/node.hpp"
#include "zio/util.hpp"
#include "zio/main.hpp"
#include "zio/logging.hpp"
#include "zio/flow.hpp"
#include "zio/sml.hpp"

#include <string>
#include <deque>
#include <queue>
#include <iostream>

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



// Event types

struct SendMsg {
    zio::Message& msg;
};
struct RecvMsg {
    zio::Message& msg;
};

struct FlushPay {};
struct BeginFlow {};
// struct CheckCredit {};


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
        zio::debug("[flow {}] check_pay no credit attr '{}'",
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

auto penultimate_credit = [](auto e, FlowApp& f) {
    zio::debug("[flow {}] penultimate_credit {}/{}",
               f.name(), f.credit, f.total_credit);
    return f.total_credit - f.credit == 1;
};

auto check_one_credit = [](auto e, FlowApp& f) {
    zio::debug("[flow {}] check_one_credit {}/{}",
               f.name(), f.credit, f.total_credit);
    return f.credit == 1;
};

auto check_many_credit = [](auto e, FlowApp& f) {
    zio::debug("[flow {}] check_one_credit {}/{}",
               f.name(), f.credit, f.total_credit);
    return f.credit > 1;
};

// Actions

auto send_msg = [](auto e, FlowApp& f) {
    ++ f.send_seqno;
    e.msg.set_seqno(f.send_seqno);
    e.msg.set_remote_id(f.remid);
    f.port->send(e.msg);
    zio::debug("[flow {}] send_msg #{} with {}/{} credit, {}",
               f.name(), f.send_seqno, f.credit, f.total_credit, e.msg.label());
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
        zio::debug("[flow {}] flush_pay no credit to flush",
                   f.port->name());
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

namespace {

// main states
struct CTOR{};
struct IDLE{};
struct BOTSEND{};
struct BOTRECV{};
struct FLOWING{};
struct READY{};

struct FINACK{};
struct ACKFIN{};
struct FIN{};

// giving states
// struct GIVING{};
struct BROKE{};
struct GENEROUS{};
// struct CREDITCHECK{};

// taking states
// struct TAKING{};
// struct WALLETCHECK{};
struct RICH{};
struct HANDSOUT{};

struct flowsm_taking {
    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
            * state<RICH> + event<FlushPay> [have_credit] / flush_pay = state<HANDSOUT>
            , state<HANDSOUT> + event<RecvMsg> [ penultimate_credit and check_dat] / recv_msg = state<RICH>
            , state<HANDSOUT> + event<RecvMsg> [!penultimate_credit and check_dat] / recv_msg = state<HANDSOUT>
            , state<HANDSOUT> + event<FlushPay> [have_credit] / flush_pay = state<HANDSOUT>
            // * state<TAKING> = state<WALLETCHECK>
            // , state<WALLETCHECK> + event<FlushPay> [have_credit] / flush_pay = state<HANDSOUT>
            // , state<HANDSOUT>    + event<RecvMsg>  [check_dat]  / recv_msg = state<WALLETCHECK>
            );
    }
};

struct flowsm_giving {
    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
            * state<BROKE> + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            , state<GENEROUS> + event<SendMsg> [ check_one_credit and check_dat] / send_msg = state<BROKE>
            , state<GENEROUS> + event<SendMsg> [check_many_credit and check_dat] / send_msg = state<GENEROUS>
            , state<GENEROUS> + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            // * state<GIVING> = state<BROKE>
            // , state<BROKE>    + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            // , state<GENEROUS> + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            // , state<GENEROUS> + event<SendMsg> [check_dat] / send_msg = state<CREDITCHECK>
            // , state<CREDITCHECK> + event<CheckCredit> [ have_credit] = state<GENEROUS>
            // , state<CREDITCHECK> + event<CheckCredit> [!have_credit] = state<BROKE>
            );
    }
};

struct flowsm_flowing {
    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
            * state<FLOWING> = state<READY>
            , state<READY> + event<BeginFlow> [ is_giver ] = state<flowsm_giving>
            , state<READY> + event<BeginFlow> [!is_giver ] = state<flowsm_taking>
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

, state<BOTSEND> + event<RecvMsg> [check_recv_bot] / recv_bot = state<flowsm_flowing>
, state<BOTRECV> + event<SendMsg> [check_send_bot] / send_msg = state<flowsm_flowing>

, state<flowsm_flowing> + event<SendMsg> [check_eot] / send_msg = state<ACKFIN>
, state<flowsm_flowing> + event<RecvMsg> [check_eot] / recv_msg = state<FINACK>

, state<FINACK> + event<SendMsg> [check_eot] / send_msg = state<FIN>
, state<ACKFIN> + event<RecvMsg> [check_eot] / recv_msg = state<FIN>

            );
    }
};

struct flow_logger {
    template <class SM, class TEvent>
    void log_process_event(const TEvent&) {
        std::string e = boost::sml::aux::get_type_name<TEvent>();
        const std::string a = "anonymous";
        if (e.size() > a.size()) {
            if (e.substr(e.size() - a.size()) == a) {
                return;
            }
        }
        zio::debug("[{}][process_event] {}",
                   boost::sml::aux::get_type_name<SM>(),
                   boost::sml::aux::get_type_name<TEvent>());
    }

    template <class SM, class TGuard, class TEvent>
    void log_guard(const TGuard&, const TEvent&, bool result) {
        zio::debug("[{}][guard] {} {} {}",
                   boost::sml::aux::get_type_name<SM>(),
                   boost::sml::aux::get_type_name<TGuard>(),
                   boost::sml::aux::get_type_name<TEvent>(),
                   (result ? "[OK]" : "[Reject]"));
    }

    template <class SM, class TAction, class TEvent>
    void log_action(const TAction&, const TEvent&) {
        zio::debug("[{}][action] {} {}",
                   boost::sml::aux::get_type_name<SM>(),
                   boost::sml::aux::get_type_name<TAction>(),
                   boost::sml::aux::get_type_name<TEvent>());
    }

    template <class SM, class TSrcState, class TDstState>
    void log_state_change(const TSrcState& src, const TDstState& dst) {
        zio::debug("[{}][transition] {} -> {}",
                   boost::sml::aux::get_type_name<SM>(),
                   src.c_str(), dst.c_str());
    }
};

template <class T>
void flow_dump_plantuml() noexcept {
  auto src_state = std::string{boost::sml::aux::string<typename T::src_state>{}.c_str()};
  auto dst_state = std::string{boost::sml::aux::string<typename T::dst_state>{}.c_str()};
  if (dst_state == "X") {
    dst_state = "[*]";
  }

  if (T::initial) {
    std::cout << "[*] --> " << src_state << std::endl;
  }

  std::cout << src_state << " --> " << dst_state;

  const auto has_event = !boost::sml::aux::is_same<typename T::event, boost::sml::anonymous>::value;
  const auto has_guard = !boost::sml::aux::is_same<typename T::guard, boost::sml::front::always>::value;
  const auto has_action = !boost::sml::aux::is_same<typename T::action, boost::sml::front::none>::value;

  if (has_event || has_guard || has_action) {
    std::cout << " :";
  }

  if (has_event) {
    std::cout << " " << boost::sml::aux::get_type_name<typename T::event>();
  }

  if (has_guard) {
    std::cout << " [" << boost::sml::aux::get_type_name<typename T::guard::type>() << "]";
  }

  if (has_action) {
    std::cout << " / " << boost::sml::aux::get_type_name<typename T::action::type>();
  }

  std::cout << std::endl;
}

template <template <class...> class T, class... Ts>
void flow_dump_plantuml(const T<Ts...>&) noexcept {
  int _[]{0, (flow_dump_plantuml<Ts>(), 0)...};
  (void)_;
}

template <class SM>
void flow_plantuml(const SM&) noexcept {
  std::cout << "@startuml" << std::endl << std::endl;
  flow_dump_plantuml(typename SM::transitions{});
  std::cout << std::endl << "@enduml" << std::endl;
}

typedef boost::sml::sm<flowsm,
                       boost::sml::logger<flow_logger>,
                       boost::sml::defer_queue<std::deque>,
                       boost::sml::process_queue<std::queue>
                       > FlowSM;

} // namespace


void test_dump_plantuml()
{
    FlowApp app;
    boost::sml::sm<flowsm> sm{app};
    flow_plantuml(sm);
    boost::sml::sm<flowsm_flowing> sm_flowing{app};
    flow_plantuml(sm_flowing);

    boost::sml::sm<flowsm_taking> sm_taking{app};
    flow_plantuml(sm_taking);

    boost::sml::sm<flowsm_giving> sm_giving{app};
    flow_plantuml(sm_giving);
    
}

// A protototype for a user-facing class which takes care of the
// high-level parts of flow protocol.  For simplicity, it will
// assert() in many places.  The real class will throw.
struct FlowDevice {
    zio::portptr_t port;
    FlowApp app;
    flow_logger logger;
    FlowSM sm;
    std::string direction;
    int credit;

    zio::Message msg{"FLOW"};

    void clear() {
        msg.set_label_object({});
    }
    std::string flowtype() {
        auto fobj = msg.label_object();
        return fobj["flow"];
    }

    FlowDevice(zio::portptr_t p, std::string dir, int credit)
        : port{p}, app{port}, sm{logger,app}, direction{dir}, credit{credit} { }

    // Exchange BOT with other end
    void bot_handshake() {
        assert (sm.is(boost::sml::state<IDLE>));
        if (zio::sock_type(port->socket()) == ZMQ_SERVER) {
            bot_handshake_serverish();
        }
        else {
            bot_handshake_clientish();
        }
        assert (sm.is< decltype(boost::sml::state<flowsm_flowing>) >(boost::sml::state<READY>));
        // boost::sml::state<READY>));
        assert(app.send_seqno == 0);
        assert(app.recv_seqno == 0);
        zio::debug("[flow {}] bot handshake complete", port->name());
        assert(sm.process_event(BeginFlow{}));
        zio::debug("[flow {}] enter flow", port->name());
    }

    void bot_handshake_serverish() {
        
        zio::debug("[flow {}] serverish bot handshake",
                   port->name());

        // server recv's then sends
        assert(recv());
        assert(sm.is(boost::sml::state<BOTRECV>));
        auto fobj = msg.label_object();
        std::string typ = fobj["flow"];
        assert (typ == "BOT");
        std::string other_dir = fobj["direction"];
        if (direction == "inject") {
            assert (other_dir == "extract");
        }
        else {
            assert (other_dir == "inject");
        }
        int other_credit = fobj["credit"];
        // protocol lets server override client's credit request,
        // here we just test for overall consistency.
        assert(credit == other_credit);
        fobj["direction"] = direction;
        msg.set_label_object(fobj);

        auto ok = sm.process_event(SendMsg{msg});
        assert(ok);
    }

    void bot_handshake_clientish() {
        msg.set_label_object({{"flow","BOT"},
                              {"direction",direction},
                              {"credit",credit}});
        zio::debug("[flow {}] clientish bot handshake label:{}",
                   port->name(),  msg.label());

        assert(sm.is(boost::sml::state<IDLE>));

        bool ok = sm.process_event(SendMsg{msg});
        assert(ok);
        assert(sm.is(boost::sml::state<BOTSEND>));

        zio::debug("[flow {}] clientish bot send okay", port->name());

        ok = recv();            // get BOT reply
        assert(ok);
        auto fobj = msg.label_object();
        std::string typ = fobj["flow"];
        assert (typ == "BOT");
        std::string other_dir = fobj["direction"];
        if (direction == "inject") {
            assert (other_dir == "extract");
        }
        else {
            assert (other_dir == "inject");
        }
        int other_credit = fobj["credit"];
        credit = other_credit;
    }

    void sendeot() {            // call if receive EOT
        std::string pname = port->name();
        sm.visit_current_states([pname](auto state) {
                                    zio::debug("[flow {}] sendeot from {}", pname, state.c_str());
                                });

        msg.set_label_object({{"flow","EOT"}});
        auto ok = sm.process_event(SendMsg{msg});
        assert(ok);
        assert(sm.is(boost::sml::state<ACKFIN>) or sm.is(boost::sml::state<FIN>));
    }
    void shutdown(zio::timeout_t timeout={}) { // call to initiate shutdown handshake
        zio::debug("[flow {}] shutdown", port->name());
        sendeot();
        do {
            auto what = recv(timeout);
            if (!what) {
                zio::debug("[flow {}] shutdown timeout on EOT reply", port->name());
                break;
            }
        } while (! sm.is(boost::sml::state<FINACK>));
    }
    void sendpay() {
        zio::debug("[flow {}] sendpay", port->name());
        sm.process_event(FlushPay{}); // don't care if fails
    }
    void senddat() {
        std::string pname = port->name();
        sm.visit_current_states([pname](auto state) {
                                    zio::debug("[flow {}] senddat from {}", pname, state.c_str());
                                });

        msg.set_label_object({{"flow","DAT"}});
        auto ok = sm.process_event(SendMsg{msg});
        assert(ok);
        // sm.process_event(CheckCredit{});
    }

        
    // Try to receive one message and process it through the state
    // machine.  Return nullptr if timeout of unexpected message
    // otherwise return "DAT", "PAY" or "EOT".  If the latter, handle
    // responding.
    const char* recv_proc(zio::timeout_t timeout = {}) {
        if (direction == "inject") {
            sendpay();
        }
        if (! recv(timeout)) {
            return nullptr;
        }
        std::string typ = flowtype();
        if (direction == "inject") {
            if (typ == "DAT") {
                return "DAT";
            }
            if (typ == "PAY") {
                return nullptr;
            }
        }
        if (direction == "extract") {
            if (typ == "DAT") {
                return nullptr;
            }
            if (typ == "PAY") {
                return "PAY";
            }
        }
        if (typ == "EOT") {
            sendeot();
            return "EOT";
        }
        
        throw std::runtime_error("unexpected flow type " + typ);
    }


    // Recv a message on flow port and run it thought the state
    // machine.  Return false if message is bogus
    [[nodiscard]]
    bool recv(zio::timeout_t timeout={}) {
        clear();
        {
            auto ok = port->recv(msg, timeout);
            if (!ok) {
                zio::debug("[flow {}] recv timeout", port->name());
                return false;
            }
        }
        {
            auto ok = sm.process_event(RecvMsg{msg});
            if (!ok) {
                zio::debug("[flow {}] sm recv fail for: {}",
                           port->name(), msg.label());
                return false;
            }
        }
        return true;
    }


};


static
void flow_endpoint(zio::socket_t& link, int socket, bool sender, int credit)
{
    // actor ready
    link.send(zio::message_t{}, zio::send_flags::none);

    // create node based on socket/sender config
    std::string nodename = "client";
    std::string othernode = "server";
    if (socket == ZMQ_SERVER) {
        nodename = "server";
        othernode = "client";
    }
    std::string direction = "inject";
    std::string portname = "recver";
    std::string otherport = "sender";
    if (sender) {
        direction = "extract";
        portname = "sender";
        otherport = "recver";
    }

    zio::debug("[{} {}] direction: {}, socket: {}, sender: {}, credit: {}",
               nodename, portname, direction, zio::sock_type_name(socket), sender, credit);


    zio::Node node(nodename);
    auto port = node.port(portname, socket);
    if (nodename == "server") { // note, client can bind.
        port->bind();           // here, simplify cfg.
    }                           
    else {
        port->connect(othernode, otherport);
    }
    node.online();

    FlowDevice device(port, direction, credit);
    device.bot_handshake();

    zio::poller_t<> link_poller;
    link_poller.add(link, zio::event_flags::pollin);

    std::vector<zio::poller_event<>> events(1);

    while (true) {
        zio::debug("[{} {}] main loop", nodename, direction);
        if (link_poller.wait_all(events, zio::time_unit_t{0})) {
            zio::debug("[{} {}] link hit", nodename, direction);
            device.shutdown(zio::timeout_t(1000));
            break;
        }

        if (sender and device.app.credit) {
            device.senddat();
        }

        const char* what = device.recv_proc(zio::timeout_t(100));
        if (!what) {
            continue;           // timeout or bogus message
        }
        if (what == std::string("EOT")) {
            zio::debug("[{} {}] got EOT", nodename, direction);
            break;
        }
        if (what == std::string("DAT")) {
            zio::debug("[{} {}] got DAT", nodename, direction);
            // a real application would do something useful here.
            continue;
        }

    }

    zio::debug("[{} {}] node going offline", nodename, direction);
    node.offline();

    zio::debug("[{} {}] waiting for actor shutdown", nodename, direction);
    zio::message_t rmsg;
    auto res = link.recv(rmsg);
    assert(res);
}

#include "zio/actor.hpp"

// this test makes better uses of the FlowDevice
void test_concise_c2s()
{
    zio::context_t ctx;

    const int credit = 10;
    zio::debug("test_concise_c2s start actors with {} credit", credit);
    // int socket, bool sender, int credit)
    zio::zactor_t one(ctx, flow_endpoint, ZMQ_SERVER, false, credit);
    zio::zactor_t two(ctx, flow_endpoint, ZMQ_CLIENT, true,  credit);

    zio::debug("test_concise_c2s sleep");
    zio::sleep_ms(zio::time_unit_t{1000});
    zio::debug("test_concise_c2s shutdown actors");
    zio::message_t signal;
    one.link().send(signal, zio::send_flags::none);
    two.link().send(signal, zio::send_flags::none);
    zio::debug("test_concise_c2s sleep again");
    zio::sleep_ms(zio::time_unit_t{100});
    zio::debug("test_concise_c2s exit");
}

void test_longhand()
{
    using namespace boost;

    // this test interleaves client and server conversation

    zio::Node snode("server", 1);
    auto sport = snode.port("recver", ZMQ_SERVER);
    sport->bind();
    snode.online();
    zio::Node cnode("client", 2);
    auto cport = cnode.port("sender", ZMQ_CLIENT);
    cport->connect("server","recver");
    cnode.online();

    // this test only "half uses" the device see test_concise() for
    // more leaning-in.
    const int credit = 2;
    FlowDevice server(sport, "inject", credit);
    FlowDevice client(cport, "extract", credit);

    assert(server.sm.is(sml::state<IDLE>));
    assert(server.app.send_seqno == -1);

    assert(client.sm.is(sml::state<IDLE>));
    assert(client.app.send_seqno == -1);

    // CLIENT: initial BOT
    {
        zio::Message cbot("FLOW");
        cbot.set_label_object({{"flow","BOT"},
                               {"direction","extract"},
                               {"credit",2}});

        auto ok = client.sm.process_event(SendMsg{cbot});
        assert(ok);
        assert(client.sm.is(sml::state<BOTSEND>));
        assert(client.app.send_seqno == 0);
    }
    
    // CLIENT: try to send again.  This apparently quietly does
    // nothing.  Is there a way to get some error?
    {
        zio::debug("client: try double send");

        zio::Message cbot("FLOW");
        cbot.set_label_object({{"flow","BOT"},
                               {"direction","extract"},
                               {"credit",2}});

        auto ok = client.sm.process_event(SendMsg{cbot});
        assert(!ok);
        assert(client.sm.is(sml::state<BOTSEND>));
        assert(client.app.send_seqno == 0);
    }

    // SERVER: do a recv, better be a BOT
    {
        assert(server.recv());
        {
            assert(server.sm.is(sml::state<BOTRECV>));
            assert(server.app.recv_seqno == 0);
        }
        {
            zio::debug("server: try double recv");
            auto ok = server.sm.process_event(RecvMsg{server.msg});
            assert(!ok);
            assert(server.sm.is(sml::state<BOTRECV>));
            assert(server.app.recv_seqno == 0);
            zio::debug("server: double recv successfully blocked");
        }

        // SERVER responds to BOT, partly reuse last message
        auto fobj = server.msg.label_object();
        zio::debug("server bot: {}", fobj.dump());
        assert(fobj["direction"].get<std::string>() == "extract");
        fobj["direction"] = "inject"; // server
        server.msg.set_label_object(fobj);

        auto ok = server.sm.process_event(SendMsg{server.msg});
        assert(ok);
        //assert(server.sm.is(sml::state<READY>));
        assert (server.sm.is< decltype(sml::state<flowsm_flowing>) >(sml::state<READY>));
        assert(server.app.send_seqno == 0);
        assert(!server.app.giver);
    }

    // CLIENT do a recv, better be a bot
    {
        assert(client.recv());
        // assert(client.sm.is(sml::state<READY>));
        assert (client.sm.is< decltype(sml::state<flowsm_flowing>) >(sml::state<READY>));
        assert(client.app.recv_seqno == 0);
        assert(client.app.giver);
    }

    // Explicitly leave READY state
    {
        auto ok = server.sm.process_event(BeginFlow{});
        assert(ok);
        assert(server.sm.is< decltype(sml::state<flowsm_taking>) >(sml::state<HANDSOUT>));
    }

    {
        auto ok = client.sm.process_event(BeginFlow{});
        assert(ok);
        assert(client.sm.is< decltype(sml::state<flowsm_giving>) >(sml::state<BROKE>));
    }


    // both are now in flow
    
    {
        assert(client.recv());
        assert(client.sm.is< decltype(sml::state<flowsm_giving>) >(sml::state<GENEROUS>));
    }

    {
        zio::Message cdat("FLOW");
        cdat.set_label_object({{"flow","DAT"}});

        auto ok = client.sm.process_event(SendMsg{cdat});
        assert(ok);
        assert(client.sm.is< decltype(sml::state<flowsm_giving>) >(sml::state<GENEROUS>));
    }

    {
        assert(server.recv());
        assert(server.sm.is< decltype(sml::state<flowsm_taking>) >(sml::state<HANDSOUT>));
    }

    // shut it down

    {                           // client
        zio::Message ceot("FLOW");
        ceot.set_label_object({{"flow","EOT"}});
        auto ok = client.sm.process_event(SendMsg{ceot});
        assert(ok);
        assert(client.sm.is(sml::state<ACKFIN>));
    }

    {                           // server
        assert(server.recv());
        assert(server.sm.is(sml::state<FINACK>));
        {
            auto ok = server.sm.process_event(SendMsg{server.msg});
            assert(ok);
            assert(server.sm.is(sml::state<FIN>));
        }
    }

    {                           // client
        assert(client.recv());
        assert(client.sm.is(sml::state<FIN>));
    }    
}

int main()
{
    zio::init_all();


    // test_longhand();
    test_concise_c2s();
    // test_dump_plantuml();

    return 0;
}
