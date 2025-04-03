#include <eventbus/event_bus.hpp>
#include <iostream>
#include <string>

const char new_line = '\n';

struct first_event {
    std::string message;
};

struct second_event {
    int id;
    std::string message;
};

struct third_event {
    double value{234.00};
};

void free_function_event_handler(const first_event& evt) {
    std::cout << "event: " << evt.message << std::endl;
}

class first_event_callback_object {
  public:
    first_event_callback_object() = default;
    void on_event_fired(const first_event&) { event_count_++; }
    void on_third_event() {};
    [[nodiscard]] int get_event_count() const { return event_count_; }

  private:
    int event_count_{0};
};

class third_event_callback_object {
  public:
    void on_third_event() {};
};

class internal_registration_class {
    dp::handler_registration reg;

  public:
    /// CTAD not allowed in non-static struct member so we have to include the empty brackets
    internal_registration_class(dp::event_bus<>& bus)
        : reg(std::move(bus.register_handler([](const first_event& evt) {
              std::cout << "test class: " << evt.message << "\n";
          }))) {}
};

int main() {
    using namespace dp;
    event_bus evt_bus;

    // register free function
    const auto reg = evt_bus.register_handler(&free_function_event_handler);
    const auto third_event_reg = evt_bus.register_handler([](const third_event& evt) {
        std::cout << "my third event handler: " << evt.value << new_line;
    });

    const auto third_event_reg_2 = evt_bus.register_handler<third_event>(
        []() { std::cout << "I just do stuff when a third_event is fired." << new_line; });

    internal_registration_class test_object(evt_bus);

    first_event evt{"hello from first event"};
    evt_bus.fire_event(evt);
    evt_bus.fire_event(third_event{13.0});
    evt_bus.remove_handler(third_event_reg);
    evt_bus.fire_event(third_event{13.0});

    const auto other_event_reg = evt_bus.register_handler([](const second_event& other_evt) {
        std::cout << "first other event handler says: " << other_evt.message << std::endl;
    });
    const auto other_event_second_reg = evt_bus.register_handler([](const second_event& other_evt) {
        std::cout << "second other event handler says: " << other_evt.id << " " << other_evt.message
                  << std::endl;
    });
    const auto dmy_evt_first_reg = evt_bus.register_handler([](const first_event& dmy_evt) {
        std::cout << "third event handler says: " << dmy_evt.message << std::endl;
    });

    first_event_callback_object callback_obj;
    const auto dmy_evt_pmr_reg =
        evt_bus.register_handler(&callback_obj, &first_event_callback_object::on_event_fired);
    const auto thrid_event_reg_pmr = evt_bus.register_handler<third_event>(
        &callback_obj, &first_event_callback_object::on_third_event);

    // the following does not compile
    // third_event_callback_object teo;
    // const auto rg = evt_bus.register_handler<third_event>(&teo,
    // &first_event_callback_object::on_third_event);

    second_event snd_evt{2, "hello there from second event"};
    first_event first_evt{"another first event"};

    evt_bus.fire_event(first_evt);

    std::cout << "Firing second event\n";
    evt_bus.fire_event(snd_evt);
    evt_bus.remove_handler(other_event_reg);
    std::cout << "Firing second and third event\n";
    evt_bus.fire_event(snd_evt);
    evt_bus.fire_event(third_event{});

    std::cout << "callback count: " << callback_obj.get_event_count() << std::endl;
    std::cout << "handler count: " << evt_bus.handler_count() << "\n";
    std::cout << "removing handlers..."
              << "\n";

    // this is optional as the handler registrations are RAII objects
    evt_bus.remove_handler(reg);
    evt_bus.remove_handler(other_event_second_reg);
    evt_bus.remove_handler(third_event_reg_2);
    evt_bus.remove_handler(dmy_evt_first_reg);
    evt_bus.remove_handler(dmy_evt_pmr_reg);
    evt_bus.remove_handler(thrid_event_reg_pmr);

    return 0;
}
