#include "glTFtoVK.hpp"

VkFilter glTF::toVk::magFilter(uint32_t magFilter)
{
    switch(magFilter)
    {
    case 9728:
        return VK_FILTER_NEAREST;
    case 9729:
        return VK_FILTER_LINEAR;
    default:
        // TODO: WARN
        return VK_FILTER_LINEAR;
    }
}

VkFilter glTF::toVk::minFilter(uint32_t minFilter)
{
    switch(minFilter)
    {
    case 9728:
        return VK_FILTER_NEAREST;
    case 9984:
        return VK_FILTER_NEAREST;
    case 9986:
        return VK_FILTER_NEAREST;
    case 9729:
        return VK_FILTER_LINEAR;
    case 9985:
        return VK_FILTER_LINEAR;
    case 9987:
        return VK_FILTER_LINEAR;
    default:
        // TODO: WARN
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode glTF::toVk::mipmapMode(uint32_t minFilter)
{
    if(minFilter == 9986 || minFilter == 9987)
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode glTF::toVk::addressMode(uint32_t wrap)
{
    switch(wrap)
    {
    case 33071:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case 33648:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case 10497:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    default:
        // TODO: WARN
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}
