#pragma once

#include "Shaders.hpp"

#include <string_view>
#include <vector>

std::vector<uint32_t> compileHLSL(std::string_view path, Shaders::Stage stage);