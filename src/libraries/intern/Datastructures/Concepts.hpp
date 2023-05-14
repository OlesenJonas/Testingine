#pragma once

#include <type_traits>

// ------- isContained ----

template <typename T, typename... List>
struct isContained;

template <typename T, typename Head, typename... Tail>
struct isContained<T, Head, Tail...>
{
    constexpr static bool value = std::is_same<T, Head>::value || isContained<T, Tail...>::value;
};

template <typename T>
struct isContained<T>
{
    constexpr static bool value = false;
};

// ------- isDistinct ----

template <typename... List>
struct isDistinct;

template <typename Head, typename... Tail>
struct isDistinct<Head, Tail...>
{
    constexpr static bool value = !isContained<Head, Tail...>::value && isDistinct<Tail...>::value;
};

template <>
struct isDistinct<>
{
    constexpr static bool value = true;
};