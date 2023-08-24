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

    template <typename T, typename... Ts>
    struct TypeIndex_s;

    template <typename T, typename... Ts>
    struct TypeIndex_s<T, T, Ts...> : std::integral_constant<std::size_t, 0>
    {
    };

    template <typename T, typename U>
    struct TypeIndex_s<T, U> : std::integral_constant<std::size_t, 1>
    {
    };

    template <typename T, typename U, typename... Ts>
    struct TypeIndex_s<T, U, Ts...> : std::integral_constant<std::size_t, 1 + TypeIndex_s<T, Ts...>::value>
    {
    };

    /*
        If not found, returns sizeof...(Ts)
    */
    template <typename T, typename... Ts>
    constexpr std::size_t TypeIndex = TypeIndex_s<T, Ts...>::value;

    static_assert(TypeIndex<float, float> == 0);
    static_assert(TypeIndex<float, float, char> == 0);
    static_assert(TypeIndex<float, char, float> == 1);
    static_assert(TypeIndex<float, char, char> == 2);

    template <typename T, typename... Ts>
    constexpr bool TypeInPack = TypeIndex<T, Ts...> < sizeof...(Ts);

    static_assert(TypeInPack<float, float>);
    static_assert(!TypeInPack<float, char>);
    static_assert(TypeInPack<float, float, char>);
    static_assert(TypeInPack<float, char, float>);
    static_assert(!TypeInPack<float, char, char>);

} // namespace PoolHelper