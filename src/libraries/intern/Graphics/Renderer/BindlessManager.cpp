#include "Graphics/Renderer/BindlessManager.hpp"
#include "VulkanDebug.hpp"
#include "VulkanRenderer.hpp"
#include <intern/ResourceManager/ResourceManager.hpp>
#include <vulkan/vulkan_core.h>

BindlessManager::BindlessManager(VulkanRenderer& renderer) : renderer(renderer){};

void BindlessManager::init()
{
    // Setting up bindless -----------------------------

    std::vector<VkDescriptorPoolSize> descriptorSizes = {
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = ResourceManager::samplerLimit,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE).limit,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE).limit,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER).limit,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER).limit,
        },
    };

    VkDescriptorPoolCreateInfo poolCrInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,

        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = (uint32_t)descriptorSizes.size(),
        .poolSizeCount = (uint32_t)descriptorSizes.size(),
        .pPoolSizes = descriptorSizes.data(),
    };
    assertVkResult(vkCreateDescriptorPool(renderer.device, &poolCrInfo, nullptr, &bindlessDescriptorPool));

    for(const auto& entry : descriptorTypeTable)
    {
        // constexpr VkDescriptorBindingFlags defaultDescBindingFlag =
        //     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        //     VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
        constexpr VkDescriptorBindingFlags defaultDescBindingFlag =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        std::vector<VkDescriptorBindingFlags> descBindingFlags = {defaultDescBindingFlag};

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = entry.first,
                .descriptorCount = entry.second.limit,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr,
            },
        };

        if(entry.first == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
        {
            descBindingFlags.push_back(defaultDescBindingFlag);

            setLayoutBindings[0].binding = ResourceManager::samplerLimit;

            setLayoutBindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = ResourceManager::samplerLimit,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr,
            });
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo layoutBindingFlagsCrInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = (uint32_t)descBindingFlags.size(),
            .pBindingFlags = descBindingFlags.data(),
        };

        VkDescriptorSetLayoutCreateInfo descSetLayoutCrInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &layoutBindingFlagsCrInfo,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = (uint32_t)setLayoutBindings.size(),
            .pBindings = setLayoutBindings.data(),
        };
        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(renderer.device, &descSetLayoutCrInfo, nullptr, &layout);
        setDebugName(layout, (std::string{entry.second.debugName} + "_setLayout").c_str());
        bindlessSetLayouts[entry.second.setIndex] = layout;

        // todo: could allocate all at once
        VkDescriptorSet set;
        VkDescriptorSetAllocateInfo setAllocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = bindlessDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };
        vkAllocateDescriptorSets(renderer.device, &setAllocInfo, &set);
        setDebugName(set, (std::string{entry.second.debugName} + "_Set").c_str());
        bindlessDescriptorSets[entry.second.setIndex] = set;
    }

    renderer.deleteQueue.pushBack([=]()
                                  { vkDestroyDescriptorPool(renderer.device, bindlessDescriptorPool, nullptr); });
    for(auto setLayout : bindlessSetLayouts)
    {
        renderer.deleteQueue.pushBack([=]()
                                      { vkDestroyDescriptorSetLayout(renderer.device, setLayout, nullptr); });
    }
}

uint32_t BindlessManager::createUniformBufferBinding(VkBuffer buffer)
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

    vkUpdateDescriptorSets(renderer.device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}

uint32_t BindlessManager::createStorageBufferBinding(VkBuffer buffer)
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

    vkUpdateDescriptorSets(renderer.device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}

uint32_t BindlessManager::createSampledImageBinding(VkImageView view, VkImageLayout layout)
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
        .dstBinding = ResourceManager::samplerLimit,
        .dstArrayElement = freeIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(renderer.device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}

uint32_t BindlessManager::createSamplerBinding(VkSampler sampler, uint32_t index)
{
    auto& tableEntry = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

    VkDescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = nullptr,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkWriteDescriptorSet setWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = bindlessDescriptorSets[tableEntry.setIndex],
        .dstBinding = 0,
        .dstArrayElement = index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(renderer.device, 1, &setWrite, 0, nullptr);

    return index;
}

uint32_t BindlessManager::createStorageImageBinding(VkImageView view, VkImageLayout layout)
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

    vkUpdateDescriptorSets(renderer.device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}