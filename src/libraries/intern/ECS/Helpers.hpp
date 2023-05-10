#pragma once

#include <cstdint>
#include <utility>

namespace ECSHelpers
{
    // from
    // https://www.reddit.com/r/gamedev/comments/d8jw41/comment/f1bm54l/?utm_source=share&utm_medium=web2x&context=3
    //  could maybe use std::type_index, but the current solution is even constexpr

    // MSVC will throw away functions with the same contents
    // at compile time as optimization, this forces them to
    // be "unique", so that they aren't removed.
    template <typename T>
    constexpr const std::uintptr_t key_var{0u};

    template <typename T>
    constexpr const std::uintptr_t* key() noexcept
    {
        return &key_var<T>;
    }

    using TypeKey = const std::uintptr_t* (*)();
    template <typename T>
    constexpr TypeKey getTypeKey() noexcept
    {
        return ECSHelpers::key<T>;
    };

    // Using these instead of per component structs with virtual functions, will need to compare
    template <typename C>
    void moveConstructComponent(void* srcObject, void* dstptr)
    {
        // need to construct at location
        C* newC = new(dstptr) C(std::move(*reinterpret_cast<C*>(srcObject)));
    }

    template <typename C>
    void destroyComponent(void* ptr)
    {
#ifndef _NDEBUG
        C* component = (C*)ptr;
#endif
        ((C*)ptr)->~C();
    }
}; // namespace ECSHelpers