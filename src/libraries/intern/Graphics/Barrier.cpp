#include "Barrier.hpp"

void insertImageMemoryBarriers(VkCommandBuffer cmd, const Span<const VkImageMemoryBarrier2> imageMemoryBarriers)
{
    const VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = uint32_t(imageMemoryBarriers.size()),
        .pImageMemoryBarriers = imageMemoryBarriers.constData(),
    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}