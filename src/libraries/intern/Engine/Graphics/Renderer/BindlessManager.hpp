#pragma once

#include <Datastructures/DynamicBitset.hpp>
#include <Datastructures/Span.hpp>
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

    uint32_t getDescriptorSetsCount() const;
    const VkDescriptorSet* getDescriptorSets();
    const VkDescriptorSetLayout* getDescriptorSetLayouts();

    uint32_t createSamplerBinding(VkSampler sampler, uint32_t index);
    enum class ImageUsage
    {
        Sampled,
        Storage,
        Both,
    };
    uint32_t createImageBinding(VkImageView view, ImageUsage possibleImageUsages);
    enum class BufferUsage
    {
        Uniform,
        Storage,
    };
    uint32_t createBufferBinding(VkBuffer buffer, BufferUsage possibleBufferUsage);

  private:
    // todo: check against limit from physicalDeviceProperties.limits
    static const uint32_t sampledImagesLimit = 128;
    static const uint32_t storageImagesLimit = 128;
    static const uint32_t uniformBuffersLimit = 128;
    static const uint32_t storageBuffersLimit = 128;

    struct BindlessSet
    {
        uint32_t setIndex = 0xFFFFFFFF;
        uint32_t limit = 0xFFFFFFFF;
        std::string_view debugName;
        DynamicBitset freeIndices;
    };

    std::unordered_map<VkDescriptorType, BindlessSet> descriptorTypeTable = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         {0, sampledImagesLimit, "SampledImages", DynamicBitset{sampledImagesLimit}}},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         {1, storageImagesLimit, "StorageImages", DynamicBitset{storageImagesLimit}}},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         {2, uniformBuffersLimit, "UniformBuffers", DynamicBitset{uniformBuffersLimit}}},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         {3, storageBuffersLimit, "StorageBuffers", DynamicBitset{storageBuffersLimit}}},
    };

    VkDescriptorPool bindlessDescriptorPool;
    std::array<VkDescriptorSetLayout, 4> bindlessSetLayouts;
    std::array<VkDescriptorSet, 4> bindlessDescriptorSets;

    VulkanRenderer& renderer;
};