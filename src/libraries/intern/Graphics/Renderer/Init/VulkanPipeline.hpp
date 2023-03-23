#pragma once

#include <vector>

#include <Datastructures/Span.hpp>
#include <vulkan/vulkan_core.h>

struct VulkanPipeline
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
        // TODO: use dynamic state for viewport and scissor, dont bake into pipeline!
        //       see HelloTriangleApplication
        VkViewport viewport;
        VkRect2D scissor;
        bool depthTest = false;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineLayout pipelineLayout;
    };

    VulkanPipeline(const CreateInfo&& info);

    VkPipeline createPipeline(
        VkDevice device,
        const Span<const VkFormat> colorAttachmentFormats,
        VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCrInfos;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCrInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCrInfo;

    VkViewport viewport;
    VkRect2D scisscor;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCrInfo;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCrInfo;

    VkPipelineMultisampleStateCreateInfo multisampleStateCrInfo;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;

    VkPipelineLayout pipelineLayout;
};