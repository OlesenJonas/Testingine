#pragma once

#include <cstdint>

// from https://twitter.com/molecularmusing/status/1643175739896766464/photo/1
template <typename T, size_t N>
constexpr uint32_t ArraySize(const T (&)[N])
{
    return N;
}