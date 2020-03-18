// https://gist.github.com/indiosmo/08ab24181770125d5a2448d27f6ae99f
// https://github.com/boost-experimental/sml/issues/185#issuecomment-412728422

#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <functional>
#include "sml.hpp"

struct client_protocol
{
    typedef std::vector<std::uint8_t> const_buffer;


  // clang-format off

  // public interface
  struct ev_connect{};
  struct ev_disconnect{};

  // connection handlers
  struct ev_error{};
  struct ev_connected{};
  struct ev_handshake_ack{};
  struct ev_disconnected {};
  struct ev_fin {};

  // stream handlers
  struct ev_retry{};
  struct ev_payload
  {
    const_buffer data;
  };

  // clang-format on

  struct events
  {
    std::function<void()> established;
    std::function<void()> disconnected;
    std::function<void()> error;
    std::function<void(const_buffer)> payload;
  };

  struct actions
  {
    std::function<void()> connect;
    std::function<void(bool initiator)> disconnect;

    std::function<void()> start_stream;
    std::function<void()> send_handshake;
    std::function<void()> wait_retry;
  };

  struct connected
  {
    auto operator()()
    {
      using namespace boost::sml;
      namespace sml = boost::sml;

      // clang-format off
      return make_transition_table(
          *"handshake"_s + sml::on_entry<_> / [](actions& ac) { ac.start_stream(); ac.send_handshake(); }
          ,"handshake"_s + sml::event<ev_handshake_ack> = "established"_s

          ,"established"_s + sml::on_entry<_> / [](events& ev){ ev.established(); }
          ,"established"_s + sml::event<ev_payload> / [](events& ev, const ev_payload& e) {
            ev.payload(e.data);
          }
        );
      // clang-format on
    }
  };

  struct protocol
  {
    auto operator()()
    {
      using namespace boost::sml;
      namespace sml = boost::sml;

      // clang-format off
      return make_transition_table(
          *"disconnected"_s + sml::event<ev_connect> = "connecting"_s 

          ,"connecting"_s + sml::on_entry<_> / [](actions& ac) { ac.connect(); }
          ,"connecting"_s + sml::event<ev_connected> = state<connected>
          ,"connecting"_s + sml::event<ev_disconnect> = "disconnecting"_s

          ,state<connected> + sml::on_exit<_> / [](events& ev) { ev.disconnected(); }
          ,state<connected> + sml::event<ev_disconnect> = "disconnecting"_s
          ,state<connected> + sml::event<ev_retry> = "waiting_retry"_s
          ,state<connected> + sml::event<ev_fin> = "fin"_s
          ,state<connected> + sml::event<ev_error> / [](actions& ac, events& ev)
          {
            ac.disconnect(false);
            ev.error();
          } = X

          ,"disconnecting"_s + sml::on_entry<_> / [](actions& ac) { ac.disconnect(true); }
          ,"disconnecting"_s + sml::event<ev_disconnected> = "disconnected"_s

          ,"waiting_retry"_s + sml::on_entry<_> / [](actions& ac) { ac.wait_retry(); }
          ,"waiting_retry"_s + sml::event<ev_connect> = "connecting"_s
          ,"waiting_retry"_s + sml::event<ev_disconnect> = "disconnecting"_s

          ,"fin"_s + sml::on_entry<_> / [](actions& ac) { ac.disconnect(false); }
      );
      // clang-format on
    }
  };

  auto operator()()
  {
    using namespace boost::sml;
    namespace sml = boost::sml;

    // clang-format off
    return make_transition_table(
        *state<protocol> + sml::event<ev_error> / [](actions& ac, events& ev)
        {
          ac.disconnect(false);
          ev.error();
        } = X
      );
    // clang-format on
  }
};


template <class T>
void dump_transition() noexcept {
    namespace sml = boost::sml;

    auto src_state = std::string{sml::aux::string<typename T::src_state>{}.c_str()};
    auto dst_state = std::string{sml::aux::string<typename T::dst_state>{}.c_str()};
    if (dst_state == "X") {
        dst_state = "[*]";
    }

    if (T::initial) {
        std::cout << "[*] --> " << src_state << std::endl;
    }

    std::cout << src_state << " --> " << dst_state;

    const auto has_event = !sml::aux::is_same<typename T::event, sml::anonymous>::value;
    const auto has_guard = !sml::aux::is_same<typename T::guard, sml::front::always>::value;
    const auto has_action = !sml::aux::is_same<typename T::action, sml::front::none>::value;

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
void dump_transitions(const T<Ts...>&) noexcept {
    int _[]{0, (dump_transition<Ts>(), 0)...};
    (void)_;
}

template <class SM>
void dump(const SM&) noexcept {
    std::cout << "@startuml" << std::endl << std::endl;
    dump_transitions(typename SM::transitions{});
    std::cout << std::endl << "@enduml" << std::endl;
}

int main()
{
    namespace sml = boost::sml;
    using namespace sml;
    client_protocol::actions ac {
        []() { std::cout << "connect\n"; },
        [](bool initiator) { std::cout << "disconnect "<<initiator<<"\n"; },
        []() { std::cout << "start_stream\n"; },
        []() { std::cout << "send_handshake\n"; },
        []() { std::cout << "wait_retry\n"; }
    };
    client_protocol::events ev {
        []() { std::cout << "established\n"; },
        []() { std::cout << "disconnected\n"; },
        []() { std::cout << "error\n"; },
        [](client_protocol::const_buffer) { std::cout << "payload\n"; }
    };

    sm<client_protocol> mysm{ev, ac};
    assert(mysm.is(state<client_protocol::protocol>));

    mysm.process_event(client_protocol::ev_connect());
    mysm.process_event(client_protocol::ev_connected());
    assert(mysm.is<decltype(state<client_protocol::connected>)>("handshake"_s));
    mysm.process_event(client_protocol::ev_handshake_ack());
    mysm.process_event(client_protocol::ev_payload{{}});
    mysm.process_event(client_protocol::ev_disconnect());
    mysm.process_event(client_protocol::ev_disconnected());
    assert(mysm.is(state<client_protocol::protocol>));

    // {
    //     dump(mysm);
    //     sm<client_protocol::connected> c{ev,ac};
    //     dump(c);
    //     sm<client_protocol::protocol> p{ev,ac};
    //     dump(p);
    // }

}
