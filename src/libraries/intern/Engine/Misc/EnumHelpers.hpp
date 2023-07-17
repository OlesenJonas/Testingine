#pragma once

#define nthBit(x) (1u << x)

#define ENUM_OPERATOR_OR(T)                                                                                       \
    inline T operator|(T lhs, T rhs)                                                                              \
    {                                                                                                             \
        static_assert(std::is_enum_v<T>);                                                                         \
        using Underlying = std::underlying_type_t<T>;                                                             \
        return static_cast<T>(static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));                       \
    }

#define ENUM_OPERATOR_AND(T)                                                                                      \
    inline T operator&(T lhs, T rhs)                                                                              \
    {                                                                                                             \
        static_assert(std::is_enum_v<T>);                                                                         \
        using Underlying = std::underlying_type_t<T>;                                                             \
        return static_cast<T>(static_cast<Underlying>(lhs) & static_cast<Underlying>(rhs));                       \
    }