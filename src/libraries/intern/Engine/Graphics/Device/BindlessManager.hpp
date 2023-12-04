#pragma once

#include <Datastructures/DynamicBitset.hpp>
#include <Datastructures/Span.hpp>
#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

class VulkanDevice;

class BindlessManager
{
  public:
    explicit BindlessManager(VulkanDevice& device);
    void init();

    uint32_t getDescriptorSetsCount() const;
    const VkDescriptorSet* getDescriptorSets();
    const VkDescriptorSetLayout* getDescriptorSetLayouts();

    uint32_t createSamplerBinding(VkSampler sampler);
    void freeSamplerBinding(uint32_t index);

    enum class ImageUsage
    {
        Sampled,
        Storage,
        Both,
    };
    uint32_t createImageBinding(VkImageView view, ImageUsage possibleImageUsages);
    void freeImageBinding(uint32_t index, ImageUsage usage);

    enum class BufferUsage
    {
        Uniform,
        Storage,
    };
    uint32_t createBufferBinding(VkBuffer buffer, BufferUsage possibleBufferUsage);
    void freeBufferBinding(uint32_t index, BufferUsage usage);

    // TODO: find a better way to determine / set this, but IDK yet
    //        Maybe pass on construction?
    //        At least seperate into compute and graphics?
    static constexpr auto maxBindlessPushConstantSize = sizeof(uint32_t) * 8;

    // todo: check against limit from physicalDeviceProperties.limits
    static constexpr uint32_t samplerLimit = 32;
    static constexpr uint32_t sampledImagesLimit = 1024;
    static constexpr uint32_t storageImagesLimit = 1024;
    static constexpr uint32_t uniformBuffersLimit = 1024;
    static constexpr uint32_t storageBuffersLimit = 4096;

  private:
    struct BindlessSet
    {
        uint32_t setIndex = 0xFFFFFFFF;
        uint32_t limit = 0xFFFFFFFF;
        std::string_view debugName;
        DynamicBitset freeIndices;
    };

    std::unordered_map<VkDescriptorType, BindlessSet> descriptorTypeTable = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, //
         {0, samplerLimit, "Samplers", DynamicBitset{samplerLimit}}},
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

    VulkanDevice& gfxDevice;
};