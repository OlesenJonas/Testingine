#include "Sampler.hpp"
#include "SamplerToVulkan.hpp"
#include <Engine/Application/Application.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>

Handle<Sampler> ResourceManager::createSampler(Sampler::Info&& info)
{
    auto* device = VulkanRenderer::get();

    // Check if such a sampler already exists

    // since the sampler limit is quite low atm, and sampler creation should be a rare thing
    // just doing a linear search here. Could of course hash the creation info and do some lookup
    // in the future

    for(uint32_t i = 0; i < freeSamplerIndex; i++)
    {
        Sampler::Info& entryInfo = samplerArray[i].info;
        if(entryInfo == info)
        {
            return Handle<Sampler>{i, 1};
        }
    }

    // Otherwise create a new sampler

    Handle<Sampler> samplerHandle{freeSamplerIndex, 1};
    freeSamplerIndex++;
    Sampler* sampler = &samplerArray[samplerHandle.index];
    assert(sampler == get(samplerHandle));

    VkSamplerCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = toVkFilter(info.magFilter),
        .minFilter = toVkFilter(info.minFilter),
        .mipmapMode = toVkMipmapMode(info.mipMapFilter),
        .addressModeU = toVkAddressMode(info.addressModeU),
        .addressModeV = toVkAddressMode(info.addressModeV),
        .addressModeW = toVkAddressMode(info.addressModeW),
        .mipLodBias = 0.0f,
        .anisotropyEnable = info.enableAnisotropy,
        .maxAnisotropy = 16.0f, // TODO: retrieve from hardware capabilities
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_LESS,
        .minLod = info.minLod,
        .maxLod = info.maxLod,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    sampler->info = info;

    vkCreateSampler(device->device, &createInfo, nullptr, &sampler->sampler);

    // TODO: this should go directly through device! forget the ->bindlessManager.
    sampler->resourceIndex = device->bindlessManager.createSamplerBinding(sampler->sampler, samplerHandle.index);

    return samplerHandle;
}

bool Sampler::Info::operator==(const Sampler::Info& rhs) const
{
#define EQUAL_MEMBER(member) member == rhs.member
    return EQUAL_MEMBER(magFilter) &&        //
           EQUAL_MEMBER(minFilter) &&        //
           EQUAL_MEMBER(mipMapFilter) &&     //
           EQUAL_MEMBER(addressModeU) &&     //
           EQUAL_MEMBER(addressModeV) &&     //
           EQUAL_MEMBER(addressModeW) &&     //
           EQUAL_MEMBER(enableAnisotropy) && //
           EQUAL_MEMBER(minLod) &&           //
           EQUAL_MEMBER(maxLod);
#undef EQUAL_MEMBER
}