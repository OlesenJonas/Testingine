#pragma once

// enables using std::unordered_map with string keys and string_view + char* for lookup
// https://en.cppreference.com/w/cpp/container/unordered_map/find

#include <string>
#include <string_view>
#include <unordered_map>

// using
struct StringHash
{
    using hashType = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator()(const char* str) const
    {
        return hashType{}(str);
    }
    std::size_t operator()(std::string_view str) const
    {
        return hashType{}(str);
    }
    std::size_t operator()(std::string const& str) const
    {
        return hashType{}(str);
    }
};