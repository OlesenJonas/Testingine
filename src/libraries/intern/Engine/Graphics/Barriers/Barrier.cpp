// TODO: should probably be part of vulkan specific directory

#include "Barrier.hpp"
#include <Engine/Engine.hpp>
#include <Graphics/Texture/TexToVulkan.hpp>

void submitBarriers(VkCommandBuffer cmd, Span<const Barrier> barriers)
{
    static std::vector<VkImageMemoryBarrier2> imageBarriers;
    static std::vector<VkBufferMemoryBarrier2> bufferBarriers;

    for(const auto& barrier : barriers)
    {
        if(barrier.type == Barrier::Type::Buffer)
        {
            assert(false);
        }
        else if(barrier.type == Barrier::Type::Image)
        {
            auto* rm = Engine::get()->getResourceManager();
            auto* renderer = Engine::get()->getRenderer();

            const Texture* tex = rm->get(barrier.image.texture);
            if(tex == nullptr)
            {
                BREAKPOINT;
                continue; // TODO: LOG warning
            }
            assert(tex->descriptor.mipLevels > 0);
            int32_t mipCount = barrier.image.mipCount == Texture::MipLevels::All
                                   ? tex->descriptor.mipLevels - barrier.image.mipLevel
                                   : barrier.image.mipCount;

            VkImageMemoryBarrier2 vkBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,

                .srcStageMask = toVkPipelineStage(barrier.image.stateBefore),
                .srcAccessMask = toVkAccessFlags(barrier.image.stateBefore),

                .dstStageMask = toVkPipelineStage(barrier.image.stateAfter),
                .dstAccessMask = toVkAccessFlags(barrier.image.stateAfter),

                .oldLayout = toVkImageLayout(barrier.image.stateBefore),
                .newLayout = toVkImageLayout(barrier.image.stateAfter),

                .srcQueueFamilyIndex = renderer->graphicsAndComputeQueueFamily,
                .dstQueueFamilyIndex = renderer->graphicsAndComputeQueueFamily,

                .image = tex->image,
                .subresourceRange =
                    {
                        .aspectMask = toVkImageAspect(tex->descriptor.format),
                        .baseMipLevel = static_cast<uint32_t>(barrier.image.mipLevel),
                        .levelCount = static_cast<uint32_t>(mipCount),
                        .baseArrayLayer = static_cast<uint32_t>(barrier.image.arrayLayer),
                        .layerCount = static_cast<uint32_t>(
                            tex->descriptor.type == Texture::Type::tCube ? 6 * barrier.image.arrayLength
                                                                         : barrier.image.arrayLength),
                    },
            };

            imageBarriers.emplace_back(vkBarrier);
        }
        else
        {
            assert(false);
        }
    }

    if(imageBarriers.empty() && bufferBarriers.empty())
        return;

    const VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = uint32_t(bufferBarriers.size()),
        .pBufferMemoryBarriers = bufferBarriers.data(),
        .imageMemoryBarrierCount = uint32_t(imageBarriers.size()),
        .pImageMemoryBarriers = imageBarriers.data(),
    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    bufferBarriers.clear();
    imageBarriers.clear();
}