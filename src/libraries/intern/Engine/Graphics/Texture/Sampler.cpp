#include "Sampler.hpp"
#include <Engine/Application/Application.hpp>
#include <Engine/Graphics/Device/VulkanConversions.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>

Handle<Sampler> ResourceManager::createSampler(Sampler::Info&& info)
{
    auto* gfxDevice = VulkanDevice::get();

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
    sampler->sampler = gfxDevice->createSampler(info);
    sampler->info = info;
    assert(sampler == get(samplerHandle));

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