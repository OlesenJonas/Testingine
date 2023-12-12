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

#include <vector>
template <typename Inner>
constexpr size_t SizeInBytes(const std::vector<Inner>& container) noexcept
{
    return container.size() * sizeof(container[0]);
}

#include <array>
template <typename Inner, size_t N>
constexpr size_t SizeInBytes(const std::array<Inner, N>& container) noexcept
{
    return container.size() * sizeof(container[0]);
}

#include "Span.hpp"
#include <span>
template <typename Inner>
constexpr size_t SizeInBytes(const Span<Inner> container) noexcept
{
    return container.size() * sizeof(container[0]);
}
template <typename Inner, size_t N>
constexpr size_t SizeInBytes(const std::span<Inner> container) noexcept
{
    return container.size() * sizeof(container[0]);
}