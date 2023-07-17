#include <Engine/Graphics/Texture/TexToVulkan.hpp>
#include <Engine/Graphics/Texture/Texture.hpp>

int main()
{
    // Texture conversions --------------------------

    // clang-format off
    static_assert(toVkUsageSingle(Texture::Usage::Sampled) == VK_IMAGE_USAGE_SAMPLED_BIT);
    static_assert(toVkUsageSingle(Texture::Usage::Storage) == VK_IMAGE_USAGE_STORAGE_BIT);
    static_assert(toVkUsageSingle(Texture::Usage::ColorAttachment) == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    static_assert(toVkUsageSingle(Texture::Usage::DepthStencilAttachment) == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    // clang-format on

    // clang-format off
    assert(toVkUsage(Texture::Usage::Sampled) == VK_IMAGE_USAGE_SAMPLED_BIT);
    assert(toVkUsage(Texture::Usage::Storage) == VK_IMAGE_USAGE_STORAGE_BIT);
    assert(toVkUsage(Texture::Usage::ColorAttachment) == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    assert(toVkUsage(Texture::Usage::DepthStencilAttachment) == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    assert(toVkUsage(Texture::Usage::TransferDst) == VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    assert(toVkUsage(Texture::Usage::TransferSrc) == VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    {
        Texture::Usage usage = Texture::Sampled |Texture::Storage;
        VkImageUsageFlags vkUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        assert(toVkUsage(usage) == vkUsage);
    }
    {
        Texture::Usage usage = Texture::Storage |Texture::ColorAttachment;
        VkImageUsageFlags vkUsage = VK_IMAGE_USAGE_STORAGE_BIT |VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        assert(toVkUsage(usage) == vkUsage);
    }
    {
        Texture::Usage usage = Texture::Storage | Texture::ColorAttachment | Texture::DepthStencilAttachment;
        VkImageUsageFlags vkUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        assert(toVkUsage(usage) == vkUsage);
    }
    // clang-format on
}