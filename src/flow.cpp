#include "zio/flow.hpp"
#include "zio/util.hpp"
#include "zio/logging.hpp"
#include "zio/sml.hpp"

#include <string>
#include <deque>
#include <queue>

namespace zio {

    // Used by SM guards and actions and forms base class for
    // implementation of user Flow API class.  It knows nothing about
    // sockets nor the state machine itself.
    struct FlowFSM {
        FlowFSM(flow::direction_e d, size_t credit)
            : dir(d)
            , total_credit(credit) {}
        virtual ~FlowFSM() {}

        flow::direction_e dir;
        size_t total_credit;
        size_t credit{0}; 
        std::string remid{""};
        int send_seqno{-1};
        int recv_seqno{-1};

        virtual std::string name() const = 0;

        bool giver() const {
            return dir == flow::direction_e::extract;
        }
        bool taker() const {
            return dir == flow::direction_e::inject;
        }

        bool check_recv_bot(zio::Message& msg) {
            if (recv_seqno != -1) {
                zio::debug("[flow {}] check_recv_bot recv_seqno={}",
                           name(), recv_seqno);
                return false;
            }
            zio::json fobj = msg.label_object();
            std::string typ = fobj["flow"].get<std::string>();
            if (typ != "BOT") {
                zio::debug("[flow {}] check_recv_bot called with '{}'",
                           name(), msg.label());
                return false;
            }
            // Direction must be opposite from what we are.
            std::string sdir = fobj["direction"].get<std::string>();
            if (giver() and sdir == "extract") {
                zio::debug("[flow {}] check_recv_bot both are 'extract': '{}'",
                           name(), msg.label());
                return false;
            }
            if (taker() and sdir == "inject") {
                zio::debug("[flow {}] check_recv_bot both are 'inject': '{}'",
                           name(), msg.label());
                return false;
            }

            zio::debug("[flow {}] check_recv_bot okay with '{}'",
                       name(), msg.label());
            return true;
        }

        bool check_send_bot(zio::Message& msg) {
            if (send_seqno != -1) {
                zio::debug("[flow {}] check_send_bot send_seqno={}",
                           name(), send_seqno);
                return false;
            }
    
            zio::json fobj = msg.label_object();
            std::string typ = fobj["flow"].get<std::string>();
            if (typ != "BOT") {
                zio::debug("[flow {}] check_send_bot called with '{}'",
                           name(), msg.label());
                return false;
            }
            // Direction must be what we are.
            std::string sdir = fobj["direction"].get<std::string>();
            if (taker() and sdir == "extract") {
                zio::debug("[flow {}] check_send_bot we take but claim 'extract': '{}'",
                           name(), msg.label());
                return false;
            }
            if (giver() and sdir == "inject") {
                zio::debug("[flow {}] check_send_bot we give but claim 'inject': '{}'",
                           name(), msg.label());
                return false;
            }
            zio::debug("[flow {}] check_send_bot okay with '{}'",
                       name(), msg.label());
            return true;
        }

        bool check_pay(zio::Message& msg) {
            zio::json fobj = msg.label_object();
            std::string typ = fobj["flow"].get<std::string>();
            if (typ != "PAY") {
                zio::debug("[flow {}] check_pay not PAY '{}'",
                           name(), msg.label());
                return false;
            }
            if (! fobj["credit"].is_number()) {
                zio::debug("[flow {}] check_pay no credit attr '{}'",
                           name(), msg.label());
                return false;
            }
            int got_credit = fobj["credit"];
            if (got_credit + credit > total_credit) {
                zio::debug("[flow {}] check_pay too much PAY {} + {} > {}",
                           name(), got_credit, credit, total_credit);
                return false;
            }
            zio::debug("[flow {}] check_dat okay with '{}'",
                       name(), msg.label());
            return true;
        }

        bool check_dat(zio::Message& msg) {
            zio::json fobj = msg.label_object();
            std::string typ = fobj["flow"].get<std::string>();
            if (typ != "DAT") {
                zio::debug("[flow {}] check_dat not DAT '{}'",
                           name(), msg.label());
                return false;
            }
            zio::debug("[flow {}] check_dat okay with '{}'",
                       name(), msg.label());
            return true;
        }            
        bool check_eot(zio::Message& msg) {
            zio::json fobj = msg.label_object();
            std::string typ = fobj["flow"].get<std::string>();
            if (typ != "EOT") {
                zio::debug("[flow {}] check_eot not EOT '{}'",
                           name(), msg.label());
                return false;
            }
            zio::debug("[flow {}] check_eot okay with '{}'",
                       name(), msg.label());
            return true;
        }
                    
        // c/s switch
        virtual void init_credit(int other_credit) = 0;

        void recv_bot(zio::Message& msg) {
            zio::json fobj = msg.label_object();
            std::string dir = fobj["direction"];
            init_credit(fobj["credit"].get<int>());
            if (dir == "extract") { 
                total_credit = credit;
                credit = total_credit;
            }
            else if (dir == "inject") {
                total_credit = credit;
                credit = 0;          
            }
            ++ recv_seqno;
            remid = msg.remote_id();
            zio::debug("[flow {}] recv_bot #{} as {} with {}/{} credit",
                       name(), recv_seqno, dir, credit, total_credit);
        }
        void send_msg(zio::Message& msg) {
            ++ send_seqno;
            msg.set_seqno(send_seqno);
            msg.set_remote_id(remid);
            // f.port->send(e.msg);
            zio::debug("[flow {}] send_msg #{} with {}/{} credit, {}",
                       name(), send_seqno, credit, total_credit, msg.label());
        };


    };
}



// Event types
struct SendMsg {
    zio::Message& msg;
};
struct RecvMsg {
    zio::Message& msg;
};

struct FlushPay {
    zio::Message& msg;
};
struct BeginFlow {};

// Guards

auto check_recv_bot = [](auto e, zio::FlowFSM& f) { return f.check_recv_bot(e.msg); };
auto check_send_bot = [](auto e, zio::FlowFSM& f) { return f.check_send_bot(e.msg); };
auto check_pay = [](auto e, zio::FlowFSM& f) { return f.check_pay(e.msg); };
auto check_dat = [](auto e, zio::FlowFSM& f) { return f.check_dat(e.msg); };
auto check_eot = [](auto e, zio::FlowFSM& f) { return f.check_eot(e.msg); };

auto is_giver = [](auto e, zio::FlowFSM& f) { return f.giver(); };

auto have_credit = [](auto e, zio::FlowFSM& f) {
    zio::debug("[flow {}] have_credit {}/{}",
               f.name(), f.credit, f.total_credit);
    return f.credit > 0;
};

// return if we are down to our last buck
auto last_credit = [](auto e, zio::FlowFSM& f) {
    zio::debug("[flow {}] last_credit {}/{}",
               f.name(), f.credit, f.total_credit);
    return f.total_credit - f.credit == 1;
};

auto check_one_credit = [](auto e, zio::FlowFSM& f) {
    zio::debug("[flow {}] check_one_credit {}/{}",
               f.name(), f.credit, f.total_credit);
    return f.credit == 1;
};

auto check_many_credit = [](auto e, zio::FlowFSM& f) {
    zio::debug("[flow {}] check_one_credit {}/{}",
               f.name(), f.credit, f.total_credit);
    return f.credit > 1;
};

// Actions

auto send_msg = [](auto e, zio::FlowFSM& f) { f.send_msg(e.msg); };
auto send_dat = [](auto e, zio::FlowFSM& f) {
    -- f.credit;
    f.send_msg(e.msg);
};

auto recv_bot = [](auto e, zio::FlowFSM& f) {
};

auto recv_pay = [](auto e, zio::FlowFSM& f) {
    zio::json fobj = e.msg.label_object();
    int credit = fobj["credit"];
    zio::debug("[flow {}] recv_pay #{} as {} with {}/{} credit",
               f.name(), f.recv_seqno, f.dir, credit, f.total_credit);
    f.credit += credit;
};

auto flush_pay = [](auto e, zio::FlowFSM& f) {
    if (!f.credit) {
        zio::debug("[flow {}] flush_pay no credit to flush",
                   f.name());
        return;
    }
    auto fobj = e.msg.label_object();
    fobj["flow"] = "PAY";
    fobj["credit"] = f.credit;
    e.msg.set_label_object(fobj);
    e.msg.set_seqno(++f.send_seqno);
    zio::debug("[flow {}] flush_pay #{}, credit:{}",
               f.name(), f.send_seqno, f.credit);
    f.credit=0;
    if (f.remid.size()) { e.msg.set_remote_id(f.remid); }
    // f.port->send(msg);
};

auto recv_dat = [](auto e, zio::FlowFSM& f) {
    ++ f.recv_seqno;
    ++ f.credit;
    zio::debug("[flow {}] recv_dat #{} as {} with {}/{} credit",
               f.name(), f.recv_seqno, f.dir, f.credit, f.total_credit);
};

auto recv_eot = [](auto e, zio::FlowFSM& f) {
    ++ f.recv_seqno;
    zio::debug("[flow {}] recv_eot #{} as {} with {}/{} credit",
               f.name(), f.recv_seqno, f.dir, f.credit, f.total_credit);
};


// boost.sml requires the states to be in the anonymous namespace.
namespace {

// States

// taking
struct RICH{};
struct HANDSOUT{};
struct flowsm_taking {
    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
            * state<RICH> + event<FlushPay> [have_credit] / flush_pay = state<HANDSOUT>
            , state<HANDSOUT> + event<RecvMsg> [ last_credit and check_dat] / recv_dat = state<RICH>
            , state<HANDSOUT> + event<RecvMsg> [!last_credit and check_dat] / recv_dat = state<HANDSOUT>
            , state<HANDSOUT> + event<FlushPay> [have_credit] / flush_pay = state<HANDSOUT>
            );
    }
};

// giving 
struct BROKE{};
struct GENEROUS{};
struct flowsm_giving {
    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
            * state<BROKE> + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            , state<GENEROUS> + event<SendMsg> [ check_one_credit and check_dat] / send_dat = state<BROKE>
            , state<GENEROUS> + event<SendMsg> [check_many_credit and check_dat] / send_dat = state<GENEROUS>
            , state<GENEROUS> + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            );
    }
};

// flowing
struct READY{};
struct flowsm_flowing {
    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
            * state<READY> + event<BeginFlow> [ is_giver ] = state<flowsm_giving>
            , state<READY> + event<BeginFlow> [!is_giver ] = state<flowsm_taking>
            );
    }
};

// main states
struct IDLE{};
struct BOTSEND{};
struct BOTRECV{};
struct FINACK{};
struct ACKFIN{};
struct FIN{};
struct flowsm_main {

    auto operator()() const noexcept {
        using namespace boost::sml;

        return make_transition_table(
* state<IDLE> + event<SendMsg> [check_send_bot] / send_msg = state<BOTSEND>
, state<IDLE> + event<RecvMsg> [check_recv_bot] / recv_bot = state<BOTRECV>

, state<BOTSEND> + event<RecvMsg> [check_recv_bot] / recv_bot = state<flowsm_flowing>
, state<BOTRECV> + event<SendMsg> [check_send_bot] / send_msg = state<flowsm_flowing>

, state<flowsm_flowing> + event<SendMsg> [check_eot] / send_msg = state<ACKFIN>
, state<flowsm_flowing> + event<RecvMsg> [check_eot] / recv_eot = state<FINACK>

, state<FINACK> + event<SendMsg> [check_eot] / send_msg = state<FIN>
, state<ACKFIN> + event<RecvMsg> [check_eot] / recv_eot = state<FIN>

            );
    }
};


typedef boost::sml::sm<flowsm_main,
//                       boost::sml::logger<flow_logger>,
                       boost::sml::defer_queue<std::deque>,
                       boost::sml::process_queue<std::queue>
                       > FlowSM;


} // anonymous namespace




namespace  zio {

struct FlowImp : public FlowFSM {
    zio::portptr_t port;
    timeout_t timeout;
    FlowSM sm;

    FlowImp(zio::portptr_t p, flow::direction_e dir, size_t credit, timeout_t tout)
        : FlowFSM(dir, credit)
        , port{p}
        , timeout{tout}
        , sm{(FlowFSM&)*this} {}
    virtual ~FlowImp() {}

    virtual std::string name() const { return port->name(); }

    // for client/server to implement
    virtual bool bot_handshake(zio::Message& botmsg) = 0;

    bool bot(zio::Message& botmsg) {
        if (send_seqno != -1) {
            throw flow::local_error("bot send attempt with already open flow");
        }
        if (recv_seqno != -1) {
            throw flow::local_error("bot recv attempt with already open flow");
        }
        if (!sm.is(boost::sml::state<IDLE>)) {
            throw flow::local_error("bot handshake outside of flow IDLE state");
        }
        if (! bot_handshake(botmsg)) {
            return false;
        }
        if (send_seqno != 0) {
            throw flow::remote_error("bot handshake send failed");
        }
        if (recv_seqno != 0) {
            throw flow::remote_error("bot handshake recv failed");
        }
        if (! sm.is< decltype(boost::sml::state<flowsm_flowing>) >(boost::sml::state<READY>)) {
            throw flow::remote_error("bot handshake failed to reach READY state");
        }
        if (!sm.process_event(BeginFlow{})) {
            throw flow::local_error("bot handshake failed to reach FLOW state");
        }
        return true;
    }

    // Send an EOT msg as an ack (or as an initiation);
    bool eotack(zio::Message& msg) {
        auto fobj = msg.label_object();
        fobj["flow"] = "EOT";
        msg.set_label_object(fobj);
        return send(msg);
    }

    // full eot handshake
    bool eot(zio::Message& msg) {
        if (! eotack(msg)) {
            return false;
        }
        if (! recv(msg) ) {
            return false;
        }
        if (! sm.is(boost::sml::state<FIN>)) {
            throw flow::remote_error("eot handshake failed to reach FIN state");
        }
        return true;
    }

    /// Attempt to send DAT (for givers)
    bool put(zio::Message& dat) {
        // try to recv any waiting pay
        zio::Message maybe_pay;
        if (port->recv(maybe_pay, timeout_t{0})) {
            sm.process_event(RecvMsg{maybe_pay});
            if (sm.is(boost::sml::state<FINACK>)) {
                throw flow::end_of_transmission("flow get received EOT");
            }
        }

        return send(dat);
    }

    /// Attempt to get DAT (for takers)
    bool get(zio::Message& dat) {
        zio::Message pay;
        if (sm.process_event(FlushPay{pay})) {
            port->send(pay);
        }

        bool noto = recv(dat);
        if (! noto) {
            return false;
        }
        if (sm.is(boost::sml::state<FINACK>)) {
            throw flow::end_of_transmission("flow get received EOT");
        }
        return true;
    }

    // Try to do a flow level recv and process it throught the SM
    bool recv(zio::Message& msg) {
        if (! port->recv(msg, timeout)) {
            return false;       // timeout
        }
        if (! sm.process_event(RecvMsg{msg})) {
            throw flow::remote_error("recv flow bad message " + msg.label());
        }
        return true;
    }

    // Try to do a flow level send.  Process through SM then send.
    bool send(zio::Message& msg) {
        if (! sm.process_event(SendMsg{msg})) {
            throw flow::local_error("send flow message invalid to send");
        }
        // fixme: use a pollout timeout?
        return port->send(msg);
    }

};

struct FlowImpServer : public FlowImp {

    FlowImpServer(zio::portptr_t p, flow::direction_e dir, size_t credit, timeout_t tout)
        : FlowImp(p,dir,credit,tout) { }
    virtual ~FlowImpServer() {}

    virtual bool bot_handshake(zio::Message& botmsg) {

        auto our_fobj = botmsg.label_object();
        if (! recv(botmsg)) {
            return false;
        }
        if (! sm.is(boost::sml::state<BOTRECV>)) {
            throw flow::local_error("bot server handshake failed to enter BOTRECV");
        }
        if (! send(botmsg)) {
            return false;
        }
        return true;
    }

    virtual void init_credit(int other_credit) {
        if (!other_credit) {
            other_credit = credit; // server decides
        }
        else {
            credit = other_credit; // this server accepts recomendation
        }
    }

};                          

struct FlowImpClient : public FlowImp {

    FlowImpClient(zio::portptr_t p, flow::direction_e dir, size_t credit, timeout_t tout)
        : FlowImp(p,dir,credit,tout) { }
    virtual ~FlowImpClient() {}

    virtual bool bot_handshake(zio::Message& botmsg) {

        if (! send(botmsg)) {
            return false;
        }
        if (! sm.is(boost::sml::state<BOTSEND>)) {
            throw flow::local_error("bot client handshake failed to enter BOTSEND");
        }
        if (! recv(botmsg)) {
            return false;
        }
        return true;
    }

    virtual void init_credit(int other_credit) {
        credit = other_credit;  // client accepts unconditionally
    }

};

} // namespace zio

static
std::unique_ptr<zio::FlowImp>
make_imp(zio::portptr_t p, zio::flow::direction_e dir, size_t credit, zio::timeout_t timeout)
{
    if (zio::is_serverish(p->socket())) {
        return std::make_unique<zio::FlowImpServer>(p, dir, credit, timeout);
    }
    if (zio::is_clientish(p->socket())) {
        return std::make_unique<zio::FlowImpClient>(p, dir, credit, timeout);
    }
    throw zio::flow::local_error("flow given port with unsupported socket type");
}

// Forward to implementation
zio::Flow::Flow(zio::portptr_t p, zio::flow::direction_e dir, size_t credit, zio::timeout_t timeout)
    : imp(make_imp(p, dir, credit, timeout))
{
}

void zio::Flow::set_timeout(timeout_t timeout)
{
    imp->timeout = timeout;
}
size_t zio::Flow::credit() const
{
    return imp->credit;
}
size_t zio::Flow::total_credit() const
{
    return imp->total_credit;
}
bool zio::Flow::bot() {
    zio::Message msg("FLOW");
    return imp->bot(msg);
}
bool zio::Flow::bot(zio::Message& msg)
{
    return imp->bot(msg);
}

bool zio::Flow::eotack()
{
    zio::Message msg("FLOW");
    return eotack(msg);
}
bool zio::Flow::eotack(zio::Message& msg)
{
    return imp->eotack(msg);
}

bool zio::Flow::eot()
{
    zio::Message msg("FLOW");
    return eot(msg);
}
bool zio::Flow::eot(zio::Message& msg)
{
    return imp->eot(msg);
}

bool zio::Flow::put(zio::Message& msg)
{
    return imp->put(msg);
}
bool zio::Flow::get(zio::Message& msg)
{
    return imp->get(msg);
}

bool zio::Flow::recv(zio::Message& msg)
{
    return imp->recv(msg);
}

bool zio::Flow::send(zio::Message& msg)
{
    return imp->send(msg);
}
