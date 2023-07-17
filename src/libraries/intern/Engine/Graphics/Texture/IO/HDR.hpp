#pragma once

#include "../Texture.hpp"
#include <string_view>

struct Texture;
template <typename T>
class Handle;

namespace HDR
{
    Handle<Texture> load(std::string_view path, std::string_view debugName, Texture::Usage usage);
}