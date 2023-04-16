#include "GraphicsPipeline.hpp"
#include "Graphics/Renderer/VulkanDebug.hpp"
#include "Graphics/Renderer/VulkanRenderer.hpp"
#include "Shaders.hpp"

#include <SPIRV-Reflect/spirv_reflect.h>
#include <intern/Datastructures/Span.hpp>
#include <intern/Engine/Engine.hpp>
#include <intern/ResourceManager/ResourceManager.hpp>
#include <shaderc/shaderc.h>
#include <stdint.h>
#include <tuple>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

Handle<GraphicsPipeline> ResourceManager::createGraphicsPipeline(GraphicsPipeline::CreateInfo crInfo)
{
    // todo: handle errors
    Handle<GraphicsPipeline> newPipelineHandle = graphicsPipelinePool.insert();

    // if we can guarantee that no two threads access the pools at the same time we can
    // assume that the underlying pointer wont change until this function returns
    GraphicsPipeline* pipeline = graphicsPipelinePool.get(newPipelineHandle);

    VulkanRenderer& renderer = *VulkanRenderer::get();

    std::vector<uint32_t> vertexBinary = compileGLSL(crInfo.vertexShader.sourcePath, shaderc_vertex_shader);
    std::vector<uint32_t> fragmentBinary = compileGLSL(crInfo.fragmentShader.sourcePath, shaderc_fragment_shader);

    VkPipelineLayoutCreateInfo pipelineLayoutCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
    };

    // Shader Modules ----------------
    VkShaderModuleCreateInfo vertSMcrInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = 4u * vertexBinary.size(), // size in bytes, but spirvBinary is uint32 vector!
        .pCode = vertexBinary.data(),
    };
    if(vkCreateShaderModule(renderer.device, &vertSMcrInfo, nullptr, &pipeline->vertexShader) != VK_SUCCESS)
    {
        assert(false);
    }
    VkShaderModuleCreateInfo fragmSMcrInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = 4u * fragmentBinary.size(), // size in bytes, but spirvBinary is uint32 vector!
        .pCode = fragmentBinary.data(),
    };
    if(vkCreateShaderModule(renderer.device, &fragmSMcrInfo, nullptr, &pipeline->fragmentShader) != VK_SUCCESS)
    {
        assert(false);
    }

    // Parse Shader Interface -------------

    // todo: wrap into function taking shader byte code(s) (span)? could be moved into Shaders.cpp
    //       not sure yet, only really needed in one or two places, and also only with a fixed amount of shader
    //       stages so far

    SpvReflectShaderModule vertModule;
    SpvReflectShaderModule fragModule;
    SpvReflectResult result;
    result = spvReflectCreateShaderModule(vertexBinary.size() * 4, vertexBinary.data(), &vertModule);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    result = spvReflectCreateShaderModule(fragmentBinary.size() * 4, fragmentBinary.data(), &fragModule);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // Push Constant Blocks ---------------
    // todo: currently just handles a single push constant block inside the vertex shader
    uint32_t vertPushConstantBlockCount;
    result = spvReflectEnumeratePushConstantBlocks(&vertModule, &vertPushConstantBlockCount, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    assert(vertPushConstantBlockCount <= 1);

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 1,
    };
    if(vertPushConstantBlockCount > 0)
    {
        const SpvReflectBlockVariable* vertPushConstantBlock =
            spvReflectGetPushConstantBlock(&vertModule, 0, &result);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        pushConstantRange.size = vertPushConstantBlock->padded_size;
    }

    pipelineLayoutCrInfo.pushConstantRangeCount = vertPushConstantBlockCount;
    pipelineLayoutCrInfo.pPushConstantRanges = &pushConstantRange;

    // Descriptor Sets ----------------

    // todo:
    // probably not really performance optimal, especially since a hashmap seems overkill since its really only two
    // index lookups (set and binding index) and all the limits (set and binding counts) are known upfront
    // Additionally the bindings have to be copied out of the map and into contiguous memory for the creation info
    // at the end which also could be skipped if they were stored in an array to begin with

    using SetBindings = std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>;
    std::unordered_map<uint32_t, SetBindings> descriptorSets;

    uint32_t vertDescriptorSetsCount;
    result = spvReflectEnumerateDescriptorSets(&vertModule, &vertDescriptorSetsCount, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<SpvReflectDescriptorSet*> vertDescriptorSets(vertDescriptorSetsCount);
    result = spvReflectEnumerateDescriptorSets(&vertModule, &vertDescriptorSetsCount, vertDescriptorSets.data());

    for(auto* set : vertDescriptorSets)
    {
        // Find or create entry for DescriptorSet in map
        auto setEntry = descriptorSets.find(set->set);
        if(setEntry == descriptorSets.end())
        {
            setEntry =
                descriptorSets
                    .emplace(std::piecewise_construct, std::forward_as_tuple(set->set), std::forward_as_tuple())
                    .first;
        }

        // Find and update, or create entry for binding in descriptor sets' map
        SetBindings& setBindings = setEntry->second;
        const Span<SpvReflectDescriptorBinding*> bindings(set->bindings, set->binding_count);
        for(auto* binding : bindings)
        {
            auto bindingEntry = setBindings.find(binding->binding);
            if(bindingEntry == setBindings.end())
            {
                setBindings.emplace(
                    binding->binding,
                    VkDescriptorSetLayoutBinding{
                        .binding = binding->binding,
                        .descriptorType = (VkDescriptorType)binding->descriptor_type,
                        .descriptorCount = 1, // todo:
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                        .pImmutableSamplers = nullptr,
                    });
            }
        }
    }

    uint32_t fragDescriptorSetsCount;
    result = spvReflectEnumerateDescriptorSets(&fragModule, &fragDescriptorSetsCount, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<SpvReflectDescriptorSet*> fragDescriptorSets(fragDescriptorSetsCount);
    result = spvReflectEnumerateDescriptorSets(&fragModule, &fragDescriptorSetsCount, fragDescriptorSets.data());

    for(auto* set : fragDescriptorSets)
    {
        // Find or create entry for DescriptorSet in map
        auto setEntry = descriptorSets.find(set->set);
        if(setEntry == descriptorSets.end())
        {
            setEntry =
                descriptorSets
                    .emplace(std::piecewise_construct, std::forward_as_tuple(set->set), std::forward_as_tuple())
                    .first;
        }

        // Find and update, or create entry for binding in descriptor sets' map
        SetBindings& setBindings = setEntry->second;
        const Span<SpvReflectDescriptorBinding*> bindings(set->bindings, set->binding_count);
        for(auto* binding : bindings)
        {
            auto bindingEntry = setBindings.find(binding->binding);
            if(bindingEntry == setBindings.end())
            {
                setBindings.emplace(
                    binding->binding,
                    VkDescriptorSetLayoutBinding{
                        .binding = binding->binding,
                        .descriptorType = (VkDescriptorType)binding->descriptor_type,
                        .descriptorCount = 1, // todo:
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .pImmutableSamplers = nullptr,
                    });
            }
            else
            {
                VkDescriptorSetLayoutBinding& setLayoutBinding = bindingEntry->second;
                assert(setLayoutBinding.binding == binding->binding);
                assert(setLayoutBinding.descriptorType == (VkDescriptorType)binding->descriptor_type);
                setLayoutBinding.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }
        }
    }

    // todo: cache these outside of the pipeline itself?

    std::vector<VkDescriptorSetLayout> setLayouts;
    setLayouts.reserve(descriptorSets.size());
    for(const auto& set : descriptorSets)
    {
        uint32_t setIndex = set.first;
        std::vector<VkDescriptorSetLayoutBinding> setBindings;
        setBindings.reserve(set.second.size());
        for(const auto& binding : set.second)
        {
            setBindings.emplace_back(binding.second);
        }

        const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = (uint32_t)setBindings.size(),
            .pBindings = setBindings.data(),
        };
        assertVkResult(vkCreateDescriptorSetLayout(
            renderer.device, &setLayoutCreateInfo, nullptr, &(pipeline->setLayouts[setIndex])));
        setLayouts.emplace_back(pipeline->setLayouts[setIndex]);
    }

    pipelineLayoutCrInfo.setLayoutCount = setLayouts.size();
    pipelineLayoutCrInfo.pSetLayouts = setLayouts.data();

    assertVkResult(vkCreatePipelineLayout(renderer.device, &pipelineLayoutCrInfo, nullptr, &pipeline->layout));

    // Creating the pipeline ---------------------------

    const VkPipelineShaderStageCreateInfo shaderStageCrInfos[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,

            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = pipeline->vertexShader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,

            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = pipeline->fragmentShader,
            .pName = "main",
        },
    };

    const VertexInputDescription vertexDescription = Vertex::getVertexDescription();
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,

        .vertexBindingDescriptionCount = (uint32_t)vertexDescription.bindings.size(),
        .pVertexBindingDescriptions = vertexDescription.bindings.data(),
        .vertexAttributeDescriptionCount = (uint32_t)vertexDescription.attributes.size(),
        .pVertexAttributeDescriptions = vertexDescription.attributes.data(),
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,

        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,

        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,

        .polygonMode = VK_POLYGON_MODE_FILL,

        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,

        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,

        .lineWidth = 1.0f,
    };

    const bool depthTest = true;
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,

        .depthTestEnable = depthTest ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = depthTest ? VK_TRUE : VK_FALSE, // TODO: decouple
        .depthCompareOp = depthTest ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,

        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | //
                          VK_COLOR_COMPONENT_G_BIT | //
                          VK_COLOR_COMPONENT_B_BIT | //
                          VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineViewportStateCreateInfo viewportStateCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,

        .viewportCount = 1,
        .scissorCount = 1,
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,

        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,

        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachmentState,
    };

    const Span<const VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = (uint32_t)dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    const Span<const VkFormat> colorAttachmentFormats = {renderer.swapchainImageFormat};
    const VkFormat depthAttachmentFormat = renderer.depthFormat;
    const VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size()),
        .pColorAttachmentFormats = colorAttachmentFormats.data(),
        .depthAttachmentFormat = depthAttachmentFormat,
        .stencilAttachmentFormat = stencilAttachmentFormat,
    };

    const VkGraphicsPipelineCreateInfo pipelineCrInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipelineRenderingCreateInfo,

        .stageCount = 2,
        .pStages = &shaderStageCrInfos[0],
        .pVertexInputState = &vertexInputStateCrInfo,
        .pInputAssemblyState = &inputAssemblyStateCrInfo,
        .pViewportState = &viewportStateCrInfo,
        .pRasterizationState = &rasterizationStateCrInfo,
        .pMultisampleState = &multisampleStateCrInfo,
        .pDepthStencilState = &depthStencilStateCrInfo,
        .pColorBlendState = &colorBlendStateCrInfo,
        .pDynamicState = &dynamicState,

        .layout = pipeline->layout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    if(vkCreateGraphicsPipelines(
           renderer.device, VK_NULL_HANDLE, 1, &pipelineCrInfo, nullptr, &pipeline->pipeline) != VK_SUCCESS)
    {
        std::cout << "Failed to create pipeline!" << std::endl;
        assert(false);
    }

    return newPipelineHandle;
}

void ResourceManager::free(Handle<GraphicsPipeline> handle)
{
    /*
        todo:
        Enqueue into a per frame queue
        also need to ensure renderer deletes !all! objects from per frame queue during shutdown, ignoring the
            current frame!
    */
    const GraphicsPipeline* pipeline = graphicsPipelinePool.get(handle);
    if(pipeline == nullptr)
    {
        return;
    }

    // const VmaAllocator* allocator = &Engine::get()->getRenderer()->allocator;
    // const VkBuffer vkBuffer = buffer->buffer;
    // const VmaAllocation vmaAllocation = buffer->allocation;
    // Engine::get()->getRenderer()->deleteQueue.pushBack([=]()
    //                                                    { vmaDestroyBuffer(*allocator, vkBuffer, vmaAllocation);
    //                                                    });
    // bufferPool.remove(handle);
}