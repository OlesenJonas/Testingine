#pragma once

#include "../Texture.hpp"
#include <string_view>

struct Texture;
template <typename T>
class Handle;

// TODO: why is this here?
//       Only the ResourceManager should create stuff
//       So this should probably just return a CreateInfo (including initialData)

namespace HDR
{
    Handle<Texture>
    load(std::string_view path, std::string_view debugName, ResourceStateMulti states, ResourceState initial);
}