#include <doctest/doctest.h>
#include <nanobench.h>

#include <chrono>
#include <eventbus/event_bus.hpp>
#include <string>
#include <vector>

TEST_CASE("event dispatch - std::any vs std::variant") {
    // This test compares the performance of event dispatching using std::any and std::variant
    std::vector<std::size_t> args = {1'000, 10'000, 100'000};

    using namespace std::chrono_literals;
    for (const auto& dispatch_count : args) {
        ankerl::nanobench::Bench bench;
        auto bench_title =
            std::string("event dispatch - " + std::to_string(dispatch_count) + " times");
        bench.title(bench_title).relative(true).warmup(100).minEpochIterations(1000);
        bench.timeUnit(1us, "us");

        struct event {
            int data_int;
            float data_float;
            double data_double;
            std::uint64_t data_int_large;
        };

        struct event2 {
            int data_int;
        };

        struct allocating_event {
            std::vector<std::byte> data;
            allocating_event() : data(8, std::byte{0}) {}  // 8 bytes
        };

        bench.run("std::any", [dispatch_count]() {
            dp::event_bus bus{};

            auto registration1 = bus.register_handler<event>([] {});
            auto registration2 = bus.register_handler<event2>([] {});
            ankerl::nanobench::doNotOptimizeAway(registration1);
            ankerl::nanobench::doNotOptimizeAway(registration2);

            for (std::size_t i = 0; i < dispatch_count; ++i) {
                bus.fire_event(event{});
                bus.fire_event(event2{});
            }
        });

        bench.run("std::variant", [dispatch_count]() {
            dp::event_bus<event, event2> bus{};

            auto registration1 = bus.register_handler<event>([] {});
            auto registration2 = bus.register_handler<event2>([] {});
            ankerl::nanobench::doNotOptimizeAway(registration1);
            ankerl::nanobench::doNotOptimizeAway(registration2);

            for (std::size_t i = 0; i < dispatch_count; ++i) {
                bus.fire_event(event{});
                bus.fire_event(event2{});
            }
        });
    }
}

TEST_CASE("event dispatch - std::any vs std::variant with allocating event") {
    // This test compares the performance of event dispatching using std::any and std::variant
    std::vector<std::size_t> args = {1'000, 10'000};

    using namespace std::chrono_literals;
    for (const auto& dispatch_count : args) {
        ankerl::nanobench::Bench bench;
        auto bench_title =
            std::string("event dispatch - " + std::to_string(dispatch_count) + " times");
        bench.title(bench_title).relative(true).warmup(100).minEpochIterations(1000);
        bench.timeUnit(1us, "us");

        struct allocating_event {
            std::vector<std::byte> data;
            allocating_event() : data(8, std::byte{0}) {}  // 8 bytes
        };

        bench.run("std::any", [dispatch_count]() {
            dp::event_bus bus{};

            auto registration1 = bus.register_handler<allocating_event>([] {});
            ankerl::nanobench::doNotOptimizeAway(registration1);

            for (std::size_t i = 0; i < dispatch_count; ++i) {
                bus.fire_event(allocating_event{});
            }
        });

        bench.run("std::variant", [dispatch_count]() {
            dp::event_bus<allocating_event> bus{};

            auto registration1 = bus.register_handler<allocating_event>([] {});
            ankerl::nanobench::doNotOptimizeAway(registration1);

            for (std::size_t i = 0; i < dispatch_count; ++i) {
                bus.fire_event(allocating_event{});
            }
        });
    }
}
