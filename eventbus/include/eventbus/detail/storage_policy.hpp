#pragma once

#include <any>
#include <functional>
#include <variant>

namespace dp::detail {
    struct any_event_bus_storage_policy {
        using event_type = std::any;
        using event_handler = std::function<void(event_type)>;
    };

    template <typename... EventTypes>
    struct variant_event_bus_storage_policy {
        using event_type = std::variant<std::reference_wrapper<const EventTypes>...>;
        using event_handler = std::function<void(event_type)>;
    };
}  // namespace dp::detail
