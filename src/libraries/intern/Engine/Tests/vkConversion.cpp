#include <Engine/Graphics/Device/VulkanConversions.hpp>

int main()
{
    // Texture conversions --------------------------

    // clang-format off
    static_assert(toVkImageUsageSingle(ResourceState::SampleSource) == VK_IMAGE_USAGE_SAMPLED_BIT);
    static_assert(toVkImageUsageSingle(ResourceState::Storage) == VK_IMAGE_USAGE_STORAGE_BIT);
    static_assert(toVkImageUsageSingle(ResourceState::Rendertarget) == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    static_assert(toVkImageUsageSingle(ResourceState::DepthStencilTarget) == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    // clang-format on

    // clang-format off
    assert(toVkImageUsage(ResourceState::SampleSource) == VK_IMAGE_USAGE_SAMPLED_BIT);
    assert(toVkImageUsage(ResourceState::Storage) == VK_IMAGE_USAGE_STORAGE_BIT);
    assert(toVkImageUsage(ResourceState::Rendertarget) == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    assert(toVkImageUsage(ResourceState::DepthStencilTarget) == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    assert(toVkImageUsage(ResourceState::TransferDst) == VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    assert(toVkImageUsage(ResourceState::TransferSrc) == VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    {
        ResourceStateMulti usage = ResourceState::SampleSource;
        usage |= ResourceState::Storage;
        VkImageUsageFlags vkUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        assert(toVkImageUsage(usage) == vkUsage);
    }
    {
        ResourceStateMulti usage = ResourceState::Storage;
        usage |= ResourceState::Rendertarget;
        VkImageUsageFlags vkUsage = VK_IMAGE_USAGE_STORAGE_BIT |VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        assert(toVkImageUsage(usage) == vkUsage);
    }
    {
        ResourceStateMulti usage = ResourceState::Storage | ResourceState::Rendertarget | ResourceState::DepthStencilTarget;
        VkImageUsageFlags vkUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        assert(toVkImageUsage(usage) == vkUsage);
    }
    // clang-format on
}