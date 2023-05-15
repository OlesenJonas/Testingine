#pragma once

#include <cstdint>

// from https://twitter.com/molecularmusing/status/1643175739896766464/
template <typename T, size_t N>
using ArrayReference = T (&)[N];

template <typename T, size_t N>
constexpr size_t ArraySize(const T (&)[N]) noexcept
{
    return N;
}