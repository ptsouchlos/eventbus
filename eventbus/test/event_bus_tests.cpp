#include <doctest/doctest.h>

#include <atomic>
#include <eventbus/event_bus.hpp>
#include <iostream>
#include <thread>

struct test_event_type {
    int id{-1};
    std::string event_message;
    double data_value{1.0};
};

inline std::ostream& operator<<(std::ostream& out, const test_event_type& evt) {
    out << "id: " << evt.id << " msg: " << evt.event_message << " data: " << evt.data_value;
    return out;
}

class event_handler_counter {
    std::atomic<unsigned int> event_count_{0};

  public:
    event_handler_counter() = default;
    [[nodiscard]] unsigned int get_count() const { return event_count_.load(); }
    void on_test_event() { ++event_count_; }
};

void free_function_callback(const test_event_type& type_event) {
    std::cout << "Free function callback : " << type_event << "\n";
}

TEST_CASE("lambda registration and de-registration") {
    dp::event_bus evt_bus;
    event_handler_counter counter;
    auto registration =
        evt_bus.register_handler<test_event_type>(&counter, &event_handler_counter::on_test_event);

    test_event_type test_event{1, "event message", 32.56};
    const auto lambda_one_reg =
        evt_bus.register_handler<test_event_type>([]() { std::cout << "Lambda 1\n"; });
    const auto lambda_two_reg =
        evt_bus.register_handler<test_event_type>([&test_event](const test_event_type& evt) {
            CHECK_EQ(evt.id, test_event.id);
            CHECK_EQ(evt.event_message, test_event.event_message);
            CHECK_EQ(evt.data_value, test_event.data_value);
        });

    const auto lambda_three_reg = evt_bus.register_handler<test_event_type>(
        [](test_event_type) { std::cout << "Lambda 3 take by copy.\n"; });

    // should be 4 because we register a handler in the test fixture SetUp
    REQUIRE_EQ(evt_bus.handler_count(), 4);
    evt_bus.fire_event(test_event);
    CHECK_EQ(counter.get_count(), 1);
    evt_bus.fire_event(test_event);
    CHECK_EQ(counter.get_count(), 2);

    evt_bus.remove_handler(lambda_one_reg);

    evt_bus.fire_event(test_event);
    CHECK_EQ(counter.get_count(), 3);
    CHECK_EQ(evt_bus.handler_count(), 3);

    evt_bus.remove_handler(lambda_two_reg);

    evt_bus.fire_event(test_event);
    CHECK_EQ(counter.get_count(), 4);
    CHECK_EQ(evt_bus.handler_count(), 2);

    evt_bus.remove_handler(lambda_three_reg);

    evt_bus.fire_event(test_event);
    CHECK_EQ(counter.get_count(), 5);
    CHECK_EQ(evt_bus.handler_count(), 1);
}

TEST_CASE("deregister while dispatching") {
    dp::event_bus evt_bus;
    event_handler_counter counter;
    auto registration =
        evt_bus.register_handler<test_event_type>(&counter, &event_handler_counter::on_test_event);

    struct deregister_while_dispatch_listener {
        dp::event_bus* evt_bus{nullptr};
        std::vector<dp::handler_registration>* registrations{nullptr};
        void on_event(test_event_type) {
            if (evt_bus && registrations) {
                std::for_each(registrations->begin(), registrations->end(),
                              [&](auto& reg) { evt_bus->remove_handler(reg); });
            }
        }
    };

    std::vector<dp::handler_registration> registrations;
    std::vector<deregister_while_dispatch_listener> listeners;
    for (auto i = 0; i < 20; ++i) {
        deregister_while_dispatch_listener listener;
        auto reg = evt_bus.register_handler<test_event_type>(
            &listener, &deregister_while_dispatch_listener::on_event);
        listeners.emplace_back(listener);
        registrations.emplace_back(std::move(reg));
    }

    listeners[0].evt_bus = &evt_bus;
    listeners[0].registrations = &registrations;

    for (auto i = 0; i < 40; ++i) {
        evt_bus.fire_event(test_event_type{3, "test event", 3.4});
        // add 1 because of the test fixture.
        CHECK_EQ(evt_bus.handler_count(), listeners.size() + 1);
    }

    // remove all the registrations
    for (auto& reg : registrations) {
        CHECK(evt_bus.remove_handler(reg));
    }

    CHECK_EQ(evt_bus.handler_count(), 1);
}

TEST_CASE("multi-threaded event dispatch") {
    class simple_listener {
        int index_;

      public:
        explicit simple_listener(int index) : index_(index) {}
        void on_event(const test_event_type& evt) const {
            std::cout << "simple event: " << index_ << " " << evt.event_message << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    };

    dp::event_bus evt_bus;
    simple_listener listener_one(1);
    simple_listener listener_two(2);

    auto reg_one =
        evt_bus.register_handler<test_event_type>(&listener_one, &simple_listener::on_event);
    auto reg_two =
        evt_bus.register_handler<test_event_type>(&listener_two, &simple_listener::on_event);

    event_handler_counter event_counter;
    auto event_handler_reg = evt_bus.register_handler<test_event_type>(
        &event_counter, &event_handler_counter::on_test_event);

    auto thread_one = std::thread([&evt_bus, &listener_one]() {
        for (auto i = 0; i < 5; ++i) {
            evt_bus.fire_event(test_event_type{3, "thread_one", 1.0});
        }
    });

    auto thread_two = std::thread([&evt_bus, &listener_two]() {
        for (auto i = 0; i < 5; ++i) {
            evt_bus.fire_event(test_event_type{3, "thread_two", 2.0});
        }
    });

    thread_one.join();
    thread_two.join();

    // include the event counter
    CHECK_EQ(evt_bus.handler_count(), 3);

    CHECK_EQ(event_counter.get_count(), 10);
}

TEST_CASE("auto de-register in destructor") {
    dp::event_bus evt_bus;
    event_handler_counter counter;
    {
        auto registration = evt_bus.register_handler<test_event_type>(
            &counter, &event_handler_counter::on_test_event);
    }

    CHECK_EQ(evt_bus.handler_count(), 0);
    evt_bus.fire_event(test_event_type{});
    evt_bus.fire_event(test_event_type{});
    evt_bus.fire_event(test_event_type{});
    CHECK_EQ(counter.get_count(), 0);
}
