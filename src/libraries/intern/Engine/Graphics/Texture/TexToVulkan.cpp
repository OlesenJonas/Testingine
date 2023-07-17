#include "TexToVulkan.hpp"

VkImageUsageFlags toVkUsage(Texture::Usage usage)
{
    using UnderlyingType = std::underlying_type_t<decltype(usage)>;
    static_assert(std::is_unsigned_v<UnderlyingType>);

    auto usageAsUnderlying = static_cast<UnderlyingType>(usage);
    VkImageUsageFlags vkUsage = 0;

    while(usageAsUnderlying > 0)
    {
        auto setBitIndex = std::countr_zero(usageAsUnderlying);
        UnderlyingType setBitField = 1u << setBitIndex;
        auto asEnum = static_cast<Texture::Usage>(1u << setBitIndex);

        vkUsage |= toVkUsageSingle(asEnum);

        usageAsUnderlying &= ~setBitField;
    }

    return vkUsage;
}
