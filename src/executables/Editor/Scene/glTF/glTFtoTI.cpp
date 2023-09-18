#include "glTFtoTI.hpp"
#include <cassert>

Sampler::Filter glTF::toEngine::magFilter(uint32_t magFilter)
{
    switch(magFilter)
    {
    case 9728:
        return Sampler::Filter::Nearest;
    case 9729:
        return Sampler::Filter::Linear;
    default:
        // TODO: WARN
        return Sampler::Filter::Linear;
    }
}

Sampler::Filter glTF::toEngine::minFilter(uint32_t minFilter)
{
    switch(minFilter)
    {
    case 9728:
        return Sampler::Filter::Nearest;
    case 9984:
        return Sampler::Filter::Nearest;
    case 9986:
        return Sampler::Filter::Nearest;
    case 9729:
        return Sampler::Filter::Linear;
    case 9985:
        return Sampler::Filter::Linear;
    case 9987:
        return Sampler::Filter::Linear;
    default:
        // TODO: WARN
        return Sampler::Filter::Linear;
    }
}

Sampler::Filter glTF::toEngine::mipmapMode(uint32_t minFilter)
{
    if(minFilter == 9986 || minFilter == 9987)
        return Sampler::Filter::Linear;
    return Sampler::Filter::Nearest;
}

Sampler::AddressMode glTF::toEngine::addressMode(uint32_t wrap)
{
    switch(wrap)
    {
    case 33071:
        return Sampler::AddressMode::ClampEdge;
    case 33648:
        assert(false);
    case 10497:
        return Sampler::AddressMode::Repeat;
    default:
        // TODO: WARN
        return Sampler::AddressMode::Repeat;
    }
}
