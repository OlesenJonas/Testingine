#pragma once

#include <vector>

#include <Datastructures/Span.hpp>
#include <vulkan/vulkan_core.h>

struct VulkanPipelineBuilder
{
    struct CreateInfo
    {
        struct ShaderStage
        {
            VkShaderStageFlagBits stage;
            VkShaderModule module;
        };
        const Span<const ShaderStage> shaderStages;
        VkPrimitiveTopology topology;
        VkPolygonMode polygonMode;
        bool depthTest = false;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineLayout pipelineLayout;
    };

    VulkanPipelineBuilder(const CreateInfo&& info);

    VkPipeline createPipeline(
        VkDevice device,
        const Span<const VkFormat> colorAttachmentFormats,
        VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCrInfos;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCrInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCrInfo;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCrInfo;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCrInfo;

    VkPipelineMultisampleStateCreateInfo multisampleStateCrInfo;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;

    VkPipelineLayout pipelineLayout;
};