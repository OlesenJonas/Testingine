#pragma once

#include "Shaders.hpp"

std::vector<uint32_t> compileHLSL(std::string_view path, Shaders::Stage stage);