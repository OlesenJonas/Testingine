#include "Graphics.hpp"
#include <bit>

VkImageUsageFlags toVkImageUsage(ResourceStateMulti states)
{
    using UnderlyingType = decltype(states)::U;
    static_assert(std::is_unsigned_v<UnderlyingType>);

    auto statesAsUnderlying = static_cast<UnderlyingType>(states.value);
    VkImageUsageFlags vkUsage = 0;

    while(statesAsUnderlying > 0)
    {
        auto setBitIndex = std::countr_zero(statesAsUnderlying);
        UnderlyingType setBitField = 1u << setBitIndex;
        auto asEnum = static_cast<ResourceState>(1u << setBitIndex);

        vkUsage |= toVkImageUsageSingle(asEnum);

        statesAsUnderlying &= ~setBitField;
    }

    return vkUsage;
}