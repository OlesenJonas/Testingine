#pragma once

// works only for very simple types, but better than
// waiting for std::trivially_relocatable
#include <type_traits>
template <typename T>
concept is_trivially_relocatable =
    std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>;