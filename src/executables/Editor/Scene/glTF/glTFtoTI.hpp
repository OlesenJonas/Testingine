#pragma once

#include <Engine/Graphics/Texture/Sampler.hpp>

namespace glTF::toEngine
{
    ::Sampler::Filter magFilter(uint32_t magFilter);
    ::Sampler::Filter minFilter(uint32_t minFilter);
    ::Sampler::Filter mipmapMode(uint32_t minFilter);
    ::Sampler::AddressMode addressMode(uint32_t wrap);
} // namespace glTF::toEngine