#pragma once

#include "StringHash.hpp"

#include <string>
#include <unordered_map>

template <typename T, typename KeyType = std::string>
    requires std::is_invocable_v<StringHash, KeyType>
using StringMap = std::unordered_map<KeyType, T, StringHash, std::equal_to<>>;