#include "BindlessManager.hpp"
#include "VulkanDebug.hpp"
#include "VulkanDevice.hpp"
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <vulkan/vulkan_core.h>

BindlessManager::BindlessManager(VulkanDevice& device) : gfxDevice(device){};

void BindlessManager::init()
{
    // Setting up bindless -----------------------------

    std::vector<VkDescriptorPoolSize> descriptorSizes = {
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_SAMPLER).limit,
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
    assertVkResult(vkCreateDescriptorPool(gfxDevice.device, &poolCrInfo, nullptr, &bindlessDescriptorPool));

    for(const auto& entry : descriptorTypeTable)
    {
        if(entry.first == VK_DESCRIPTOR_TYPE_SAMPLER)
            continue;

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

            setLayoutBindings[0].binding = samplerLimit;

            setLayoutBindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = samplerLimit,
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
        vkCreateDescriptorSetLayout(gfxDevice.device, &descSetLayoutCrInfo, nullptr, &layout);
        gfxDevice.setDebugName(layout, (std::string{entry.second.debugName} + "_setLayout").c_str());
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
        vkAllocateDescriptorSets(gfxDevice.device, &setAllocInfo, &set);
        gfxDevice.setDebugName(set, (std::string{entry.second.debugName} + "_Set").c_str());
        bindlessDescriptorSets[entry.second.setIndex] = set;
    }

    gfxDevice.deleteQueue.pushBack([device = gfxDevice.device, pool = bindlessDescriptorPool]()
                                   { vkDestroyDescriptorPool(device, pool, nullptr); });
    for(auto setLayout : bindlessSetLayouts)
    {
        gfxDevice.deleteQueue.pushBack([device = gfxDevice.device, layout = setLayout]()
                                       { vkDestroyDescriptorSetLayout(device, layout, nullptr); });
    }

    for(auto& entry : descriptorTypeTable)
    {
        entry.second.freeIndices.fill();
    }
}

uint32_t BindlessManager::getDescriptorSetsCount() const { return bindlessDescriptorSets.size(); }
const VkDescriptorSet* BindlessManager::getDescriptorSets() { return bindlessDescriptorSets.data(); }
const VkDescriptorSetLayout* BindlessManager::getDescriptorSetLayouts() { return bindlessSetLayouts.data(); }

uint32_t BindlessManager::createBufferBinding(VkBuffer buffer, BufferUsage possibleBufferUsage)
{
    VkDescriptorType descriptorType = possibleBufferUsage == BufferUsage::Uniform
                                          ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                          : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    auto& tableEntry = descriptorTypeTable.at(descriptorType);

    DynamicBitset& freeIndicesBitset = tableEntry.freeIndices;

    uint32_t freeIndex = freeIndicesBitset.getFirstBitSet();
    assert(freeIndex != 0xFFFFFFFF && "NO MORE FREE SLOTS LEFT IN DESCRIPTOR ARRAY!");
    assert(freeIndicesBitset.getBit(freeIndex));
    freeIndicesBitset.clearBit(freeIndex);
    assert(!freeIndicesBitset.getBit(freeIndex));

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
        .descriptorType = descriptorType,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(gfxDevice.device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}

void createImageDescriptor(
    VkDevice device,
    VkDescriptorSet set,
    uint32_t index,
    VkImageView view,
    VkDescriptorType descriptorType,
    VkImageLayout layout)
{
    VkDescriptorImageInfo imageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = view,
        .imageLayout = layout,
    };

    uint32_t binding = descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? BindlessManager::samplerLimit : 0;

    VkWriteDescriptorSet setWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = set,
        .dstBinding = binding,
        .dstArrayElement = index,
        .descriptorCount = 1,
        .descriptorType = descriptorType,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(device, 1, &setWrite, 0, nullptr);
}

uint32_t BindlessManager::createImageBinding(VkImageView view, ImageUsage possibleImageUsages)
{
    if(possibleImageUsages != ImageUsage::Both)
    {
        VkDescriptorType descriptorType = possibleImageUsages == ImageUsage::Sampled
                                              ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                                              : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        auto& tableEntry = descriptorTypeTable.at(descriptorType);

        DynamicBitset& freeIndicesBitset = tableEntry.freeIndices;

        uint32_t freeIndex = freeIndicesBitset.getFirstBitSet();
        assert(freeIndex != 0xFFFFFFFF && "NO MORE FREE SLOTS LEFT IN DESCRIPTOR ARRAY!");
        assert(freeIndicesBitset.getBit(freeIndex));
        freeIndicesBitset.clearBit(freeIndex);
        assert(!freeIndicesBitset.getBit(freeIndex));

        VkImageLayout layout = possibleImageUsages == ImageUsage::Sampled
                                   ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                   : VK_IMAGE_LAYOUT_GENERAL;

        createImageDescriptor(
            gfxDevice.device,
            bindlessDescriptorSets[tableEntry.setIndex],
            freeIndex,
            view,
            descriptorType,
            layout);

        return freeIndex;
    }

    // image could be used as both, so need to create both descriptors.
    // ensure that a slot is used thats free in both arrays, so the same index can be used

    auto& tableEntrySampled = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    auto& tableEntryStorage = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    DynamicBitset& freeIndicesBitsetSampled = tableEntrySampled.freeIndices;
    DynamicBitset& freeIndicesBitsetStorage = tableEntryStorage.freeIndices;

    DynamicBitset combinedFreeIndices = freeIndicesBitsetSampled & freeIndicesBitsetStorage;

    /*
        TODO:
            handle case where theres free slots left in both arrays, just not at the same index
            would need to re-order descriptors while ensuring currently in flight stuff doesnt break
    */

    uint32_t freeIndex = combinedFreeIndices.getFirstBitSet();
    assert(freeIndex != 0xFFFFFFFF && "NO MORE FREE SLOTS LEFT IN DESCRIPTOR ARRAYS!");
    assert(freeIndicesBitsetSampled.getBit(freeIndex));
    assert(freeIndicesBitsetStorage.getBit(freeIndex));
    freeIndicesBitsetSampled.clearBit(freeIndex);
    freeIndicesBitsetStorage.clearBit(freeIndex);
    assert(!freeIndicesBitsetSampled.getBit(freeIndex));
    assert(!freeIndicesBitsetStorage.getBit(freeIndex));

    createImageDescriptor(
        gfxDevice.device,
        bindlessDescriptorSets[tableEntrySampled.setIndex],
        freeIndex,
        view,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    createImageDescriptor(
        gfxDevice.device,
        bindlessDescriptorSets[tableEntryStorage.setIndex],
        freeIndex,
        view,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_IMAGE_LAYOUT_GENERAL);

    return freeIndex;
}

uint32_t BindlessManager::createSamplerBinding(VkSampler sampler)
{
    auto& tableEntry = descriptorTypeTable.at(VK_DESCRIPTOR_TYPE_SAMPLER);

    DynamicBitset& freeIndicesBitset = tableEntry.freeIndices;

    uint32_t freeIndex = freeIndicesBitset.getFirstBitSet();
    assert(freeIndex != 0xFFFFFFFF && "NO MORE FREE SLOTS LEFT IN DESCRIPTOR ARRAY!");
    assert(freeIndicesBitset.getBit(freeIndex));
    freeIndicesBitset.clearBit(freeIndex);
    assert(!freeIndicesBitset.getBit(freeIndex));

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
        .dstArrayElement = freeIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(gfxDevice.device, 1, &setWrite, 0, nullptr);

    return freeIndex;
}