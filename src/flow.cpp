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
    struct FlowFSM
    {
        FlowFSM(flow::direction_e d, int credit)
            : m_dir(d)
            , m_total_credit(credit)
        {
        }
        virtual ~FlowFSM() {}

        flow::direction_e m_dir;
        int m_total_credit;
        int m_credit{0};
        std::string m_remid{""};
        int m_send_seqno{-1};
        int m_recv_seqno{-1};

        virtual std::string name() const = 0;

        bool giver() const { return m_dir == flow::direction_e::extract; }
        bool taker() const { return m_dir == flow::direction_e::inject; }

        bool check_recv_bot(zio::Message& msg)
        {
            if (m_recv_seqno != -1) {
                ZIO_TRACE("[flow {}] check_recv_bot recv_seqno={}", name(),
                          m_recv_seqno);
                return false;
            }
            const flow::Label lab(msg);
            auto typ = lab.msgtype();
            if (typ != flow::msgtype_e::bot) {
                ZIO_TRACE("[flow {}] check_recv_bot called with '{}'", name(),
                          msg.label());
                return false;
            }
            auto odir = lab.direction();
            if (odir == flow::direction_e::unknown) {
                ZIO_TRACE("[flow {}] check_recv_bot corrupt message", name());
                return false;
            }
            if (odir == m_dir) {
                ZIO_TRACE("[flow {}] check_recv_bot both are direction ({})",
                          name(), m_dir);
                return false;
            }

            ZIO_TRACE("[flow {}] check_recv_bot okay with '{}'", name(),
                      msg.label());
            return true;
        }

        bool check_send_bot(zio::Message& msg)
        {
            if (m_send_seqno != -1) {
                ZIO_TRACE("[flow {}] check_send_bot send_seqno={}", name(),
                          m_send_seqno);
                return false;
            }

            const flow::Label lab(msg);
            auto typ = lab.msgtype();
            if (typ != flow::msgtype_e::bot) {
                ZIO_TRACE("[flow {}] check_send_bot called with '{}'", name(),
                          msg.label());
                return false;
            }
            auto odir = lab.direction();
            if (odir != m_dir) {
                ZIO_TRACE(
                    "[flow {}] check_send_bot try to send differing direction",
                    name());
                return false;
            }
            ZIO_TRACE("[flow {}] check_send_bot okay with '{}'", name(),
                      msg.label());
            return true;
        }

        bool check_pay(zio::Message& msg)
        {
            const flow::Label lab(msg);
            auto typ = lab.msgtype();
            if (typ != flow::msgtype_e::pay) {
                ZIO_TRACE("[flow {}] check_pay not PAY '{}'", name(),
                          msg.label());
                return false;
            }
            int got_credit = lab.credit();
            if (got_credit < 0) {
                ZIO_TRACE("[flow {}] check_pay bad credit attr '{}'", name(),
                          msg.label());
                return false;
            }
            if (got_credit + m_credit > m_total_credit) {
                ZIO_TRACE("[flow {}] check_pay too much PAY {} + {} > {}",
                          name(), got_credit, m_credit, m_total_credit);
                return false;
            }
            ZIO_TRACE("[flow {}] check_pay okay with '{}'", name(),
                      msg.label());
            return true;
        }

        bool check_dat(zio::Message& msg)
        {
            const flow::Label lab(msg);
            auto typ = lab.msgtype();
            if (typ != flow::msgtype_e::dat) {
                ZIO_TRACE("[flow {}] check_dat not DAT '{}'", name(),
                          msg.label());
                return false;
            }
            ZIO_TRACE("[flow {}] check_dat okay with '{}'", name(),
                      msg.label());
            return true;
        }
        bool check_eot(zio::Message& msg)
        {
            const flow::Label lab(msg);
            auto typ = lab.msgtype();
            if (typ != flow::msgtype_e::eot) {
                ZIO_TRACE("[flow {}] check_eot not EOT '{}'", name(),
                          msg.label());
                return false;
            }
            ZIO_TRACE("[flow {}] check_eot okay with '{}'", name(),
                      msg.label());
            return true;
        }

        // c/s switch
        virtual int accept_credit(int other_credit) = 0;

        void recv_bot(zio::Message& msg)
        {
            flow::Label lab(msg);
            auto dir = lab.direction();
            int cred = accept_credit(lab.credit());
            m_credit = 0;  // inject
            if (dir == flow::direction_e::extract) { m_credit = cred; }

            ++m_recv_seqno;
            m_remid = msg.remote_id();
            ZIO_TRACE("[flow {}] recv_bot #{} as {} with {}/{} credit", name(),
                      m_recv_seqno, m_dir, m_credit, m_total_credit);
        }
        void send_msg(zio::Message& msg)
        {
            ++m_send_seqno;
            msg.set_seqno(m_send_seqno);
            msg.set_remote_id(m_remid);
            ZIO_TRACE("[flow {}] send_msg #{} with {}/{} credit, {}", name(),
                      m_send_seqno, m_credit, m_total_credit, msg.label());
        };
    };
}  // namespace zio

// Event types
struct SendMsg
{
    zio::Message& msg;
};
struct RecvMsg
{
    zio::Message& msg;
};

struct FlushPay
{
    zio::Message& msg;
};
struct BeginFlow
{
};

// Guards mostly forward to FlowFSM

auto check_recv_bot = [](auto e, zio::FlowFSM& f) {
    return f.check_recv_bot(e.msg);
};
auto check_send_bot = [](auto e, zio::FlowFSM& f) {
    return f.check_send_bot(e.msg);
};
auto check_pay = [](auto e, zio::FlowFSM& f) { return f.check_pay(e.msg); };
auto check_dat = [](auto e, zio::FlowFSM& f) { return f.check_dat(e.msg); };
auto check_eot = [](auto e, zio::FlowFSM& f) { return f.check_eot(e.msg); };

auto is_giver = [](auto e, zio::FlowFSM& f) { return f.giver(); };

auto have_credit = [](auto e, zio::FlowFSM& f) {
    ZIO_TRACE("[flow {}] have_credit {}/{}", f.name(), f.m_credit,
              f.m_total_credit);
    return f.m_credit > 0;
};

// return if we are down to our last buck
auto check_last_credit = [](auto e, zio::FlowFSM& f) {
    ZIO_TRACE("[flow {}] check_last_credit {}/{}", f.name(), f.m_credit,
              f.m_total_credit);
    return f.m_total_credit - f.m_credit == 1;
};

auto check_one_credit = [](auto e, zio::FlowFSM& f) {
    ZIO_TRACE("[flow {}] check_one_credit {}/{}", f.name(), f.m_credit,
              f.m_total_credit);
    return f.m_credit == 1;
};

auto check_many_credit = [](auto e, zio::FlowFSM& f) {
    ZIO_TRACE("[flow {}] check_many_credit {}/{}", f.name(), f.m_credit,
              f.m_total_credit);
    return f.m_credit > 1;
};

// Actions

auto send_msg = [](auto e, zio::FlowFSM& f) { f.send_msg(e.msg); };
auto send_dat = [](auto e, zio::FlowFSM& f) {
    --f.m_credit;
    ZIO_TRACE("[flow {}] send_dat {}/{}", f.name(), f.m_credit,
              f.m_total_credit);
    f.send_msg(e.msg);
};

auto recv_bot = [](auto e, zio::FlowFSM& f) { f.recv_bot(e.msg); };

auto recv_pay = [](auto e, zio::FlowFSM& f) {
    ++f.m_recv_seqno;
    zio::json fobj = e.msg.label_object();
    int credit = fobj["credit"];
    f.m_credit += credit;
    ZIO_TRACE("[flow {}] recv_pay #{} as {} with {}/{} credit", f.name(),
              f.m_recv_seqno, f.m_dir, credit, f.m_total_credit);
};

auto flush_pay = [](auto e, zio::FlowFSM& f) {
    if (!f.m_credit) {
        // shouldn't be called
        zio::critical("[flow {}] flush_pay no credit to flush", f.name());
        return;
    }
    auto fobj = e.msg.label_object();
    fobj["flow"] = "PAY";
    fobj["credit"] = f.m_credit;
    e.msg.set_label_object(fobj);
    e.msg.set_seqno(++f.m_send_seqno);
    ZIO_TRACE("[flow {}] flush_pay #{}, credit:{}", f.name(), f.m_send_seqno,
              f.m_credit);
    f.m_credit = 0;
    if (f.m_remid.size()) { e.msg.set_remote_id(f.m_remid); }
};

auto recv_dat = [](auto e, zio::FlowFSM& f) {
    ++f.m_recv_seqno;
    ++f.m_credit;
    ZIO_TRACE("[flow {}] recv_dat #{} as {} with {}/{} credit", f.name(),
              f.m_recv_seqno, f.m_dir, f.m_credit, f.m_total_credit);
};

auto recv_eot = [](auto e, zio::FlowFSM& f) {
    ++f.m_recv_seqno;
    ZIO_TRACE("[flow {}] recv_eot #{} as {} with {}/{} credit", f.name(),
              f.m_recv_seqno, f.m_dir, f.m_credit, f.m_total_credit);
};

// boost.sml requires the states to be in the anonymous namespace.
namespace {

    // States

    // taking
    struct RICH
    {
    };
    struct HANDSOUT
    {
    };
    struct flowsm_taking
    {
        auto operator()() const noexcept
        {
            using namespace boost::sml;

            // clang-format off
        return make_transition_table(
            * state<RICH> + event<FlushPay> [have_credit] / flush_pay = state<HANDSOUT>
            , state<HANDSOUT> + event<RecvMsg> [ check_last_credit and check_dat] / recv_dat = state<RICH>
            , state<HANDSOUT> + event<RecvMsg> [!check_last_credit and check_dat] / recv_dat = state<HANDSOUT>
            , state<HANDSOUT> + event<FlushPay> [have_credit] / flush_pay = state<HANDSOUT>
            );
            // clang-format on
        }
    };

    // giving
    struct BROKE
    {
    };
    struct GENEROUS
    {
    };
    struct flowsm_giving
    {
        auto operator()() const noexcept
        {
            using namespace boost::sml;

            // clang-format off
        return make_transition_table(
            * state<BROKE> + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            , state<GENEROUS> + event<SendMsg> [ check_one_credit and check_dat] / send_dat = state<BROKE>
            , state<GENEROUS> + event<SendMsg> [check_many_credit and check_dat] / send_dat = state<GENEROUS>
            , state<GENEROUS> + event<RecvMsg> [check_pay] / recv_pay = state<GENEROUS>
            );
            // clang-format on
        }
    };

    // flowing
    struct READY
    {
    };
    struct flowsm_flowing
    {
        auto operator()() const noexcept
        {
            using namespace boost::sml;

            // clang-format off
        return make_transition_table(
            * state<READY> + event<BeginFlow> [ is_giver ] = state<flowsm_giving>
            , state<READY> + event<BeginFlow> [!is_giver ] = state<flowsm_taking>
            );
            // clang-format on
        }
    };

    // main states
    struct IDLE
    {
    };
    struct BOTSEND
    {
    };
    struct BOTRECV
    {
    };
    struct FINACK
    {
    };
    struct ACKFIN
    {
    };
    struct FIN
    {
    };
    struct flowsm_main
    {
        // main
        auto operator()() const noexcept
        {
            using namespace boost::sml;

            // clang-format off
        return make_transition_table(
* state<IDLE> + event<SendMsg> [check_send_bot] / send_msg = state<BOTSEND>
, state<IDLE> + event<RecvMsg> [check_recv_bot] / recv_bot = state<BOTRECV>

// , state<BOTSEND> + event<RecvMsg> [check_recv_bot] / recv_bot = state<flowsm_flowing>
// , state<BOTRECV> + event<SendMsg> [check_send_bot] / send_msg = state<flowsm_flowing>
, state<BOTSEND> + event<RecvMsg> [check_recv_bot] / recv_bot = state<READY>
, state<BOTRECV> + event<SendMsg> [check_send_bot] / send_msg = state<READY>

, state<READY> + event<BeginFlow> [ is_giver ] = state<flowsm_giving>
, state<READY> + event<BeginFlow> [!is_giver ] = state<flowsm_taking>

, state<flowsm_giving> + event<SendMsg> [check_eot] / send_msg = state<ACKFIN>
, state<flowsm_giving> + event<RecvMsg> [check_eot] / recv_eot = state<FINACK>
, state<flowsm_taking> + event<SendMsg> [check_eot] / send_msg = state<ACKFIN>
, state<flowsm_taking> + event<RecvMsg> [check_eot] / recv_eot = state<FINACK>

, state<FINACK> + event<SendMsg> [!check_eot] = state<FINACK>
, state<FINACK> + event<RecvMsg> [!check_eot] = state<FINACK>
, state<FINACK> + event<SendMsg> [check_eot] / send_msg = state<FIN>

, state<ACKFIN> + event<RecvMsg> [!check_eot] = state<ACKFIN>
, state<ACKFIN> + event<SendMsg> [!check_eot] = state<ACKFIN>
, state<ACKFIN> + event<RecvMsg> [check_eot] / recv_eot = state<FIN>

            );
            // clang-format on
        }
    };

    // boost::sml::logger<flow_logger>,
    // boost::sml::defer_queue<std::deque>,
    // boost::sml::process_queue<std::queue>

    typedef boost::sml::sm<flowsm_main> FlowSM;

}  // anonymous namespace

namespace zio {

    struct FlowImp : public FlowFSM
    {
        zio::portptr_t port;
        timeout_t timeout;
        FlowSM sm;

        FlowImp(zio::portptr_t p, flow::direction_e dir, int credit,
                timeout_t tout)
            : FlowFSM(dir, credit)
            , port{p}
            , timeout{tout}
            , sm{(FlowFSM&)*this}
        {
        }
        virtual ~FlowImp() {}

        virtual std::string name() const { return port->name(); }

        // for client/server to implement
        virtual bool bot_handshake(zio::Message& botmsg) = 0;

        template <typename... Args>
        std::string str(std::string form, Args... args)
        {
            std::string prefix = fmt::format("[flow {}] ", name());
            std::string body = fmt::format(form, args...);

            std::vector<std::string> snames;

            if (sm.is(boost::sml::state<flowsm_giving>)) {
                sm.visit_current_states<decltype(
                    boost::sml::state<flowsm_giving>)>(
                    [&snames](auto state) { snames.push_back(state.c_str()); });
            }
            else if (sm.is(boost::sml::state<flowsm_taking>)) {
                sm.visit_current_states<decltype(
                    boost::sml::state<flowsm_taking>)>(
                    [&snames](auto state) { snames.push_back(state.c_str()); });
            }
            else {
                sm.visit_current_states(
                    [&snames](auto state) { snames.push_back(state.c_str()); });
            }

            std::string states = "[";
            std::string comma = "";
            for (auto s : snames) {
                const std::string ns = "{anonymous}::";
                if (s.substr(0, ns.size()) == ns) { s = s.substr(ns.size()); }
                states += comma + s;
                comma = ",";
            }
            states += "] ";
            return prefix + states + body;
        }

        bool bot(zio::Message& botmsg)
        {
            ZIO_TRACE(str("bot handshake starts"));
            if (m_send_seqno != -1) {
                throw flow::local_error(
                    str("bot send attempt with already open flow"));
            }
            if (m_recv_seqno != -1) {
                throw flow::local_error(
                    str("bot recv attempt with already open flow"));
            }
            if (!sm.is(boost::sml::state<IDLE>)) {
                throw flow::local_error(
                    str("bot handshake outside of flow IDLE state"));
            }
            if (!bot_handshake(botmsg)) { return false; }
            if (m_send_seqno != 0) {
                throw flow::remote_error(str("bot handshake send failed"));
            }
            if (m_recv_seqno != 0) {
                throw flow::remote_error(str("bot handshake recv failed"));
            }
            // bool ready = sm.is< decltype(boost::sml::state<flowsm_flowing>)
            // >(boost::sml::state<READY>);
            bool ready = sm.is(boost::sml::state<READY>);
            if (!ready) {
                throw flow::remote_error(
                    str("bot handshake failed to reach READY state"));
            }
            if (!sm.process_event(BeginFlow{})) {
                throw flow::local_error(
                    str("bot handshake failed to reach FLOW state"));
            }
            ZIO_TRACE(str("bot handshake complete"));
            return true;
        }

        // Send an EOT msg as an ack (or as an initiation);
        bool eotack(zio::Message& msg)
        {
            flow::Label lab(msg);
            lab.msgtype(flow::msgtype_e::eot);
            lab.commit();
            return send(msg);
        }

        // full eot handshake
        bool eot(zio::Message& msg)
        {
            if (!eotack(msg)) { return false; }
            if (!sm.is(boost::sml::state<ACKFIN>)) {
                throw flow::remote_error(
                    str("eot handshake failed to reach ACKFIN state"));
            }

            while (true) {
                if (!recv(msg)) { return false; }
                if (sm.is(boost::sml::state<ACKFIN>)) {
                    ZIO_TRACE("eot recv non EOT {}", msg.label());
                    continue;
                }
                if (!sm.is(boost::sml::state<FIN>)) {
                    throw flow::remote_error(
                        str("eot handshake failed to reach FIN state"));
                }
                return true;
            }
        }

        /// Attempt to send DAT (for givers)
        bool put(zio::Message& dat)
        {
            recv_pay();
            if (!m_credit) {  // try harder
                zio::Message maybe_pay;
                if (!port->recv(maybe_pay, timeout)) { return false; }

                ZIO_TRACE(str("just in time income: {}", maybe_pay.label()));

                sm.process_event(RecvMsg{maybe_pay});
                if (sm.is(boost::sml::state<FINACK>)) {
                    throw flow::end_of_transmission(
                        str("flow get received EOT"));
                }
            }

            flow::Label lab(dat);
            lab.msgtype(flow::msgtype_e::dat);
            lab.commit();
            return send(dat);
        }

        /// Attempt to get DAT (for takers)
        bool get(zio::Message& dat)
        {
            send_pay();

            bool noto = recv(dat);
            if (!noto) { return false; }
            if (sm.is(boost::sml::state<FINACK>)) {
                throw flow::end_of_transmission(str("flow get received EOT"));
            }
            return true;
        }

        void recv_pay()
        {
            if (m_credit == m_total_credit) { return; }
            zio::Message maybe_pay;
            if (port->recv(maybe_pay, timeout_t{0})) {
                ZIO_TRACE(str("income: {}", maybe_pay.label()));

                sm.process_event(RecvMsg{maybe_pay});
                if (sm.is(boost::sml::state<FINACK>)) {
                    throw flow::end_of_transmission(
                        str("flow pay received EOT"));
                }
            }
        }
        void send_pay()
        {
            if (!m_credit) { return; }
            zio::Message pay("FLOW");
            flow::Label lab(pay);
            lab.msgtype(flow::msgtype_e::pay);
            lab.commit();

            if (sm.process_event(FlushPay{pay})) {
                ZIO_TRACE(str("paying: {}", pay.label()));
                port->send(pay);
            }
        }

        int pay()
        {
            if (giver()) { recv_pay(); }
            else {
                send_pay();
            }
            return m_credit;
        }

        // Try to do a flow level recv and process it throught the SM
        bool recv(zio::Message& msg)
        {
            if (!port->recv(msg, timeout)) {
                return false;  // timeout
            }

            ZIO_TRACE(str("recving: {}", msg.label()));

            if (!sm.process_event(RecvMsg{msg})) {
                throw flow::remote_error(
                    str("recv flow bad message {}", msg.label()));
            }
            return true;
        }

        // Try to do a flow level send.  Process through SM then send.
        bool send(zio::Message& msg)
        {
            msg.set_form("FLOW");
            flow::Label lab(msg);
            if (lab.msgtype() == flow::msgtype_e::unknown) {
                throw flow::local_error(str("send flow message type unknown"));
            }

            ZIO_TRACE(str("sending: {}", msg.label()));

            if (!sm.process_event(SendMsg{msg})) {
                throw flow::local_error(str("send invalid: {}", msg.label()));
            }
            // fixme: use a pollout timeout?
            return port->send(msg);
        }
    };

    struct FlowImpServer : public FlowImp
    {
        FlowImpServer(zio::portptr_t p, flow::direction_e dir, int credit,
                      timeout_t tout)
            : FlowImp(p, dir, credit, tout)
        {
        }
        virtual ~FlowImpServer() {}

        virtual bool bot_handshake(zio::Message& botmsg)
        {
            // auto our_fobj = botmsg.label_object();
            if (!recv(botmsg)) { return false; }
            if (!sm.is(boost::sml::state<BOTRECV>)) {
                throw flow::local_error(
                    str("bot server handshake failed to enter BOTRECV"));
            }
            flow::Label lab(botmsg);
            if (lab.direction() == flow::direction_e::inject) {
                lab.direction(flow::direction_e::extract);
            }
            else {
                lab.direction(flow::direction_e::inject);
            }
            lab.commit();

            if (!send(botmsg)) { return false; }
            return true;
        }

        virtual int accept_credit(int offer_credit)
        {
            // This server protects its memory by only allowing a client
            // to shink the credit.
            if (offer_credit > 0 and offer_credit < m_total_credit) {
                m_total_credit = offer_credit;
            }
            return m_total_credit;
        }
    };

    struct FlowImpClient : public FlowImp
    {
        FlowImpClient(zio::portptr_t p, flow::direction_e dir, int credit,
                      timeout_t tout)
            : FlowImp(p, dir, credit, tout)
        {
        }
        virtual ~FlowImpClient() {}

        virtual bool bot_handshake(zio::Message& botmsg)
        {
            flow::Label lab(botmsg);
            lab.msgtype(flow::msgtype_e::bot);
            lab.direction(m_dir);
            lab.credit(m_total_credit);
            lab.commit();

            if (!send(botmsg)) { return false; }
            if (!sm.is(boost::sml::state<BOTSEND>)) {
                throw flow::local_error(
                    str("bot client handshake failed to enter BOTSEND"));
            }
            if (!recv(botmsg)) { return false; }
            return true;
        }

        virtual int accept_credit(int offer_credit)
        {
            // Client must accept credit amount offered by server
            m_total_credit = offer_credit;
            return m_total_credit;
        }
    };

}  // namespace zio

static std::unique_ptr<zio::FlowImp> make_imp(zio::portptr_t p,
                                              zio::flow::direction_e dir,
                                              int credit,
                                              zio::timeout_t timeout)
{
    if (zio::is_serverish(p->socket())) {
        return std::make_unique<zio::FlowImpServer>(p, dir, credit, timeout);
    }
    if (zio::is_clientish(p->socket())) {
        return std::make_unique<zio::FlowImpClient>(p, dir, credit, timeout);
    }
    throw zio::flow::local_error(
        "flow given port with unsupported socket type");
}

// Forward to implementation
zio::Flow::Flow(zio::portptr_t p, zio::flow::direction_e dir, int credit,
                zio::timeout_t timeout)
    : imp(make_imp(p, dir, credit, timeout))
{
}

// We put these here as needed for using unique_ptr in pimpl pattern.
zio::Flow::~Flow() = default;
zio::Flow::Flow(zio::Flow&& rhs) = default;
zio::Flow& zio::Flow::operator=(zio::Flow&& rhs) = default;

void zio::Flow::set_timeout(timeout_t timeout) { imp->timeout = timeout; }
int zio::Flow::credit() const { return imp->m_credit; }
int zio::Flow::total_credit() const { return imp->m_total_credit; }
bool zio::Flow::bot()
{
    zio::Message msg("FLOW");
    return imp->bot(msg);
}
bool zio::Flow::bot(zio::Message& msg) { return imp->bot(msg); }

bool zio::Flow::eotack()
{
    zio::Message msg("FLOW");
    return eotack(msg);
}
bool zio::Flow::eotack(zio::Message& msg) { return imp->eotack(msg); }

bool zio::Flow::eot()
{
    zio::Message msg("FLOW");
    return eot(msg);
}
bool zio::Flow::eot(zio::Message& msg) { return imp->eot(msg); }

bool zio::Flow::put(zio::Message& msg) { return imp->put(msg); }
bool zio::Flow::get(zio::Message& msg) { return imp->get(msg); }

int zio::Flow::pay() { return imp->pay(); }

bool zio::Flow::recv(zio::Message& msg) { return imp->recv(msg); }

bool zio::Flow::send(zio::Message& msg) { return imp->send(msg); }

zio::flow::Label::Label(zio::Message& msg)
    : m_msg(msg)
    , m_fobj(msg.label_object())
{
}
zio::flow::Label::~Label() { commit(); }
void zio::flow::Label::commit()
{
    if (m_dirty) {
        m_msg.set_label_object(m_fobj);
        m_dirty = false;
    }
}
zio::flow::direction_e zio::flow::Label::direction() const
{
    if (!m_fobj.is_object()) { return direction_e::unknown; }
    const auto jit = m_fobj.find("direction");
    if (jit == m_fobj.end()) { return direction_e::unknown; }
    const auto jdir = *jit;
    if (!jdir.is_string()) { return direction_e::unknown; }
    std::string dir = jdir.get<std::string>();
    if (dir == "inject") { return direction_e::inject; }
    if (dir == "extract") { return direction_e::extract; }
    return direction_e::unknown;
}

void zio::flow::Label::direction(zio::flow::direction_e dir)
{
    switch (dir) {
        case direction_e::inject:
            m_fobj["direction"] = "inject";
            break;
        case direction_e::extract:
            m_fobj["direction"] = "extract";
            break;
        case direction_e::unknown:
            m_fobj.erase("direction");
            break;
    }
    m_dirty = true;
}

zio::flow::msgtype_e zio::flow::Label::msgtype() const
{
    if (!m_fobj.is_object()) { return msgtype_e::unknown; }
    const auto jit = m_fobj.find("flow");
    if (jit == m_fobj.end()) { return msgtype_e::unknown; }
    const auto jtyp = *jit;
    if (!jtyp.is_string()) { return msgtype_e::unknown; }
    std::string typ = jtyp.get<std::string>();
    if (typ == "BOT") { return msgtype_e::bot; }
    if (typ == "DAT") { return msgtype_e::dat; }
    if (typ == "PAY") { return msgtype_e::pay; }
    if (typ == "EOT") { return msgtype_e::eot; }
    return msgtype_e::unknown;
}
void zio::flow::Label::msgtype(zio::flow::msgtype_e typ)
{
    switch (typ) {
        case zio::flow::msgtype_e::bot:
            m_fobj["flow"] = "BOT";
            break;
        case zio::flow::msgtype_e::dat:
            m_fobj["flow"] = "DAT";
            break;
        case zio::flow::msgtype_e::pay:
            m_fobj["flow"] = "PAY";
            break;
        case zio::flow::msgtype_e::eot:
            m_fobj["flow"] = "EOT";
            break;
        case zio::flow::msgtype_e::unknown:
            m_fobj.erase("flow");
            break;
    }
    m_dirty = true;
}
int zio::flow::Label::credit() const
{
    if (!m_fobj.is_object()) { return -1; }
    const auto jit = m_fobj.find("credit");
    if (jit == m_fobj.end()) { return -1; }
    const auto jtyp = *jit;
    if (!jtyp.is_number()) { return -1; }
    return jtyp.get<int>();
}
void zio::flow::Label::credit(int cred)
{
    if (cred < 0) {
        m_fobj.erase("credit");
        return;
    }
    m_fobj["credit"] = cred;
    m_dirty = true;
}

std::string zio::flow::Label::str() const
{
    const char* dirs[] = {"?dir?", "INJECT", "EXTRACT"};
    const char* msgs[] = {"?msg?", "BOT", "DAT", "PAY", "EOT"};

    auto mt = msgtype();
    auto dir = direction();
    int cred = credit();
    return fmt::format("<flow mt:{} dir:{} cred:{}>", msgs[zio::enumind(mt)],
                       dirs[zio::enumind(dir)], cred);
}
