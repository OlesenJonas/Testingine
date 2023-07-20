#include "Sampler.hpp"

constexpr VkFilter toVkFilter(Sampler::Filter filter)
{
    switch(filter)
    {
    case Sampler::Filter::Nearest:
        return VK_FILTER_NEAREST;
    case Sampler::Filter::Linear:
        return VK_FILTER_LINEAR;
    }
}

constexpr VkSamplerMipmapMode toVkMipmapMode(Sampler::Filter filter)
{
    switch(filter)
    {
    case Sampler::Filter::Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case Sampler::Filter::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

constexpr VkSamplerAddressMode toVkAddressMode(Sampler::AddressMode mode)
{
    switch(mode)
    {
    case Sampler::AddressMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case Sampler::AddressMode::ClampEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}