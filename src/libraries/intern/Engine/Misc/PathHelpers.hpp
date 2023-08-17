#pragma once

#include <string_view>

namespace PathHelpers
{
    std::string_view fileName(std::string_view path);
    std::string_view extension(std::string_view path);
} // namespace PathHelpers