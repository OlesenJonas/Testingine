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
        VkViewport viewport;
        VkRect2D scissor;
        VkPipelineLayout pipelineLayout;
    };

    VulkanPipeline(const CreateInfo&& info);

    VkPipeline createPipeline(VkDevice device, VkRenderPass pass);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCrInfos;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCrInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCrInfo;

    VkViewport viewport;
    VkRect2D scisscor;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCrInfo;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;

    VkPipelineMultisampleStateCreateInfo multisampleStateCrInfo;

    VkPipelineLayout pipelineLayout;
};