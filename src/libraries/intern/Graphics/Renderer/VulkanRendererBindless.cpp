#include "VulkanRenderer.hpp"
#include <vulkan/vulkan_core.h>

uint32_t VulkanRenderer::createUniformBufferBinding(VkBuffer buffer)
{
    auto& tableEntry = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    uint32_t freeIndex = tableEntry.freeIndex;
    tableEntry.freeIndex++;

    VkDescriptorBufferInfo bufferInfo{
        .buffer = buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkWriteDescriptorSet setWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = bindlessDescriptorSets[tableEntry.setIndex],
        .dstBinding = 0,
        .dstArrayElement = freeIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}

uint32_t VulkanRenderer::createStorageBufferBinding(VkBuffer buffer)
{
    auto& tableEntry = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    uint32_t freeIndex = tableEntry.freeIndex;
    tableEntry.freeIndex++;

    VkDescriptorBufferInfo bufferInfo{
        .buffer = buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkWriteDescriptorSet setWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = bindlessDescriptorSets[tableEntry.setIndex],
        .dstBinding = 0,
        .dstArrayElement = freeIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}

uint32_t VulkanRenderer::createSampledImageBinding(VkImageView view, VkImageLayout layout)
{
    auto& tableEntry = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    uint32_t freeIndex = tableEntry.freeIndex;
    tableEntry.freeIndex++;

    VkDescriptorImageInfo imageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = view,
        .imageLayout = layout,
    };

    VkWriteDescriptorSet setWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = bindlessDescriptorSets[tableEntry.setIndex],
        .dstBinding = immutableSamplerCount,
        .dstArrayElement = freeIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}

uint32_t VulkanRenderer::createStorageImageBinding(VkImageView view, VkImageLayout layout)
{
    // todo: not yet tested or even used, just here as code placeholder

    auto& tableEntry = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    uint32_t freeIndex = tableEntry.freeIndex;
    tableEntry.freeIndex++;

    VkDescriptorImageInfo imageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = view,
        .imageLayout = layout,
    };

    VkWriteDescriptorSet setWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = bindlessDescriptorSets[tableEntry.setIndex],
        .dstBinding = 0,
        .dstArrayElement = freeIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}