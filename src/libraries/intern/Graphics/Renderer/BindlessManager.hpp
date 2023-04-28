#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

class VulkanRenderer;

class BindlessManager
{
  public:
    explicit BindlessManager(VulkanRenderer& renderer);
    void init();

    struct BindlessSetInfo
    {
        uint32_t setIndex = 0xFFFFFFFF;
        uint32_t limit = 0xFFFFFFFF;
        std::string_view debugName;
        uint32_t freeIndex = 0;
    };

    // todo: check against limit from physicalDeviceProperties.limits
    std::unordered_map<VkDescriptorType, BindlessSetInfo> descriptorTypeTable = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {0, 128, "SampledImages"}},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, {1, 128, "StorageImages"}},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {2, 128, "UniformBuffers"}},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, {3, 128, "StorageBuffers"}},
    };

    VkDescriptorPool bindlessDescriptorPool;
    std::array<VkDescriptorSetLayout, 4> bindlessSetLayouts;
    std::array<VkDescriptorSet, 4> bindlessDescriptorSets;

    uint32_t createSamplerBinding(VkSampler sampler, uint32_t index);
    uint32_t createSampledImageBinding(VkImageView view, VkImageLayout layout);
    uint32_t createStorageImageBinding(VkImageView view, VkImageLayout layout);
    uint32_t createUniformBufferBinding(VkBuffer buffer);
    uint32_t createStorageBufferBinding(VkBuffer buffer);

  private:
    VulkanRenderer& renderer;
};