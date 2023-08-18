#pragma once

#include <type_traits>

namespace PoolHelper
{
    inline constexpr uint32_t unlimited = ~(uint32_t(0u));

    template <typename T>
    concept hasRelocationHint = requires(T t) {
        {
            T::is_trivially_relocatable
        };
    };
    template <typename T>
    concept is_trivially_relocatable =
        (std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>) || hasRelocationHint<T>;
} // namespace PoolHelper