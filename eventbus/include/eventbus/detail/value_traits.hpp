#pragma once

#include <any>
#include <type_traits>

namespace dp::detail {
    template <typename T>
    struct is_any : std::false_type {};
    template <>
    struct is_any<std::any> : std::true_type {};

}  // namespace dp::detail
