#include <doctest/doctest.h>
#include <nanobench.h>

#include <chrono>
#include <eventbus/event_bus.hpp>
#include <string>
#include <vector>

TEST_CASE("event dispatch with eventbus using std::any") {
    std::vector<std::size_t> args = {1'000, 10'000, 100'000};

    using namespace std::chrono_literals;
    for (const auto& dispatch_count : args) {
        ankerl::nanobench::Bench bench;
        auto bench_title =
            std::string("event dispatch - " + std::to_string(dispatch_count) + " times");
        bench.title(bench_title)
            .warmup(100)
            .minEpochIterations(2000)
            .relative(true)
            .timeUnit(1us, "us");

        struct event {
            int data_int;
            float data_float;
            double data_double;
            std::uint64_t data_int_large;
        };

        struct event2 {
            int data_int;
        };

        bench.run("event_bus_with_any", [dispatch_count]() {
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

        bench.run("event_bus_with_variant", [dispatch_count] {
            using storage_policy = dp::variant_event_bus_storage_policy<event, event2>;
            dp::event_bus<storage_policy> bus{};
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
