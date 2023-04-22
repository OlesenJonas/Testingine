#include "Material.hpp"
#include <SPIRV-Reflect/spirv_reflect.h>
#include <intern/Graphics/Renderer/VulkanDebug.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>
#include <intern/Graphics/Shaders/Shaders.hpp>
#include <intern/ResourceManager/ResourceManager.hpp>
#include <iostream>
#include <shaderc/shaderc.h>
#include <span> //todo: replace with my span, but need natvis for debugging
#include <vulkan/vulkan_core.h>

Handle<Material> ResourceManager::createMaterial(Material::CreateInfo crInfo, std::string_view name)
{
    std::string_view fileView{crInfo.fragmentShader.sourcePath};
    auto lastDirSep = fileView.find_last_of("/\\");
    auto extensionStart = fileView.find_last_of('.');
    std::string_view matName =
        name.empty() ? fileView.substr(lastDirSep + 1, extensionStart - lastDirSep - 1) : name;

    // todo: handle naming collisions
    auto iterator = nameToMaterialLUT.find(matName);
    assert(iterator == nameToMaterialLUT.end());

    Handle<Material> newMaterialHandle = materialPool.insert();
    Material* material = get(newMaterialHandle);

    VulkanRenderer& renderer = *VulkanRenderer::get();

    std::vector<uint32_t> vertexBinary = compileGLSL(crInfo.vertexShader.sourcePath, shaderc_vertex_shader);
    std::vector<uint32_t> fragmentBinary = compileGLSL(crInfo.fragmentShader.sourcePath, shaderc_fragment_shader);

    // Shader Modules ----------------
    VkShaderModuleCreateInfo vertSMcrInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = 4u * vertexBinary.size(), // size in bytes, but spirvBinary is uint32 vector!
        .pCode = vertexBinary.data(),
    };
    if(vkCreateShaderModule(renderer.device, &vertSMcrInfo, nullptr, &material->vertexShader) != VK_SUCCESS)
    {
        assert(false);
    }
    setDebugName(material->vertexShader, (std::string{matName} + "_vertex").c_str());
    VkShaderModuleCreateInfo fragmSMcrInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = 4u * fragmentBinary.size(), // size in bytes, but spirvBinary is uint32 vector!
        .pCode = fragmentBinary.data(),
    };
    if(vkCreateShaderModule(renderer.device, &fragmSMcrInfo, nullptr, &material->fragmentShader) != VK_SUCCESS)
    {
        assert(false);
    }
    setDebugName(material->fragmentShader, (std::string{matName} + "_fragment").c_str());

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

    std::span<SpvReflectTypeDescription> internalTypes{
        vertModule._internal->type_descriptions, vertModule._internal->type_description_count};

    // todo: gather shader & material settings from custom structures in shader code

    nameToMaterialLUT.insert({std::string{matName}, newMaterialHandle});

    material->createPipeline();

    return newMaterialHandle;
}

void ResourceManager::free(Handle<Material> handle)
{
    const Material* material = materialPool.get(handle);
    if(material == nullptr)
    {
        return;
    }

    VkDevice device = VulkanRenderer::get()->device;

    if(material->pipeline != VK_NULL_HANDLE)
    {
        VulkanRenderer::get()->deleteQueue.pushBack([=]()
                                                    { vkDestroyPipeline(device, material->pipeline, nullptr); });
    }

    VulkanRenderer::get()->deleteQueue.pushBack(
        [=]()
        {
            vkDestroyShaderModule(device, material->vertexShader, nullptr);
            vkDestroyShaderModule(device, material->fragmentShader, nullptr);
        });

    materialPool.remove(handle);
}

void Material::createPipeline()
{
    // todo: check if pipeline with given parameters already exists, and just return that in case it does!

    VulkanRenderer& renderer = *VulkanRenderer::get();

    // Creating the pipeline ---------------------------

    const VkPipelineShaderStageCreateInfo shaderStageCrInfos[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,

            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,

            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShader,
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

        .layout = renderer.bindlessPipelineLayout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    if(vkCreateGraphicsPipelines(renderer.device, VK_NULL_HANDLE, 1, &pipelineCrInfo, nullptr, &pipeline) !=
       VK_SUCCESS)
    {
        std::cout << "Failed to create pipeline!" << std::endl;
        assert(false);
    }
}