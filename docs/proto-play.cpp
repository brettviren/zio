/*
 * THIS FILE IS TOTALLY GENERATED.  DO NOT EDIT
 */

#ifndef FOO_BLAH_HPP_SEEN
#define FOO_BLAH_HPP_SEEN

namespace foo {

class APIBase {
   public:
    zmq::message_t to_message(const json& obj) {
        std::vector<std::uint8_t> dat = json::to_msgpack(obj);
        return zmq::multipart_t mm(dat.data(), dat.size());
    }
    json to_object(const zmq::message_t& msg) {
        const std::vector<const std::uint8_t> vdat(msg.data<std::uint8_t>(),
                                                   msg.size());
        return json::from_msgpack(vdat);
    }
    void send_command(const json& obj) {
        auto msg = to_message(obj);
        auto res = m_cmdpipe.send(msg, zmq::send_flags::none);
    }
    json recv_reply() {
        zmq::message msg;
        auto res = m_cmdpipe.recv(msg, zmq::recv_flags::none);
        return to_object(msg);
    }
};

class Blah : public APIBase {
   public:
    /// Connect protocol socket to the given endpoint
    bool connect(std::string endpoint);

    /// Bind protocol socket to the given port number, return it
    int bind(int port);
};

}  // namespace foo

// Connect protocol socket to the given endpoint
bool foo::Blah::connect(std::string endpoint) {
    send_command({
        {"name", "connect"},
        {"endpoint", endpoint},
    });

    auto obj = recv_command();

    if (obj["name"] == "status") {
        if (obj["error"] == true) {
            raise std::runtime_error("connect failed");
        }

        return obj["ok"];
    }

    return false;
}

// Bind protocol socket to the given port number, return it
int foo::Blah::bind(int port) {
    send_command({
        {"name", "bind"},
        {"port", port},
    });

    auto obj = recv_command();

    if (obj["name"] == "status") {
        if (obj["error"] == true) {
            raise std::runtime_error("bind failed");
        }

        return obj["port"];
    }

    return -1;
}

namespace sml = boost::sml;

namespace {

struct begin {
    std::string greeting = "";
};

struct done {};

struct Quax {
    // guards
    virtual bool is_capital(const begin& event) == 0;
    virtual bool is_mean(const begin& event) == 0;
    virtual bool is_nice(const begin& event) == 0;

    // actions
    virtual void print_event(const begin& event) == 0;
    virtual void print_event(const done& event) == 0;
};

struct jigger {
    auto operator()() const noexcept {
        using namespace sml;

        const auto is_capital =
            [](Quax& dep, const auto& ev) { return dep.is_capital(ev); }

        const auto is_mean =
            [](Quax& dep, const auto& ev) { return dep.is_mean(ev); }

        const auto is_nice =
            [](Quax& dep, const auto& ev) { return dep.is_nice(ev); }

        const auto print_event =
            [](Quax& dep, const auto& ev) { dep.print_event(ev); }

        const auto print_event =
            [](Quax& dep, const auto& ev) { dep.print_event(ev); }

        // clang-format off
            return make_transition_table(
* state<start> + event<begin> [ is_nice and is_capital ] / ( print_event, print_event ) = state<waiting>
, state<start> + event<begin> [ is_mean and is_capital ] / ( print_event ) = state<done>
, state<start> + event<done> [  ] / ( print_event ) = state<done>
                );
        // clang-format on
    };
}

#endif  // FOO_BLAH_HPP_SEEN
