#include "Material.hpp"
#include <Datastructures/ArrayHelpers.hpp>
#include <Engine/Graphics/Renderer/VulkanDebug.hpp>
#include <Engine/Graphics/Renderer/VulkanRenderer.hpp>
#include <Engine/Graphics/Shaders/GLSL.hpp>
#include <Engine/Graphics/Shaders/HLSL.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <Graphics/Shaders/Shaders.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <iostream>
#include <shaderc/shaderc.h>
#include <span> //todo: replace with my span, but need natvis for debugging
#include <type_traits>
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

    std::vector<uint32_t> vertexBinary;
    std::vector<uint32_t> fragmentBinary;
    if(crInfo.vertexShader.sourceLanguage == Shaders::Language::HLSL)
        vertexBinary = compileHLSL(crInfo.vertexShader.sourcePath, Shaders::Stage::Vertex);
    else
        vertexBinary = compileGLSL(crInfo.vertexShader.sourcePath, Shaders::Stage::Vertex);

    if(crInfo.fragmentShader.sourceLanguage == Shaders::Language::HLSL)
        fragmentBinary = compileHLSL(crInfo.fragmentShader.sourcePath, Shaders::Stage::Fragment);
    else
        fragmentBinary = compileGLSL(crInfo.fragmentShader.sourcePath, Shaders::Stage::Fragment);

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

    std::span<SpvReflectDescriptorBinding> vertDescriptorBindings{
        vertModule.descriptor_bindings, vertModule.descriptor_binding_count};
    std::span<SpvReflectDescriptorBinding> fragDescriptorBindings{
        fragModule.descriptor_bindings, fragModule.descriptor_binding_count};

    SpvReflectDescriptorBinding* materialParametersBinding = nullptr;
    SpvReflectDescriptorBinding* materialInstanceParametersBinding = nullptr;
    // Find (if it exists) the material(-instance) parameter binding
    for(auto& binding : vertDescriptorBindings)
    {
        if(strcmp(binding.name, "g_ConstantBuffer_MaterialParameters") == 0)
            materialParametersBinding = &binding;
        if(strcmp(binding.name, "g_ConstantBuffer_MaterialInstanceParameters") == 0)
            materialInstanceParametersBinding = &binding;
    }
    for(auto& binding : fragDescriptorBindings)
    {
        if(strcmp(binding.name, "g_ConstantBuffer_MaterialParameters") == 0)
            materialParametersBinding = &binding;
        if(strcmp(binding.name, "g_ConstantBuffer_MaterialInstanceParameters") == 0)
            materialInstanceParametersBinding = &binding;
    }

    if(materialParametersBinding != nullptr)
    {
        material->parametersBufferSize = materialParametersBinding->block.padded_size;
        material->parameters.bufferSize = materialParametersBinding->block.padded_size;
        material->parameters.cpuBuffer = (char*)malloc(material->parameters.bufferSize);
        memset(material->parameters.cpuBuffer, 0, material->parameters.bufferSize);

        // Store offset (and size?) as LUT by name for writing parameters later
        material->parametersLUT = new std::unordered_map<std::string, ParameterInfo, StringHash, std::equal_to<>>;
        material->parameters.lut = material->parametersLUT;
        auto& LUT = *material->parametersLUT;

        std::span<SpvReflectBlockVariable> members{
            materialParametersBinding->block.members, materialParametersBinding->block.member_count};

        for(auto& member : members)
        {
            assert(member.padded_size >= member.size);

            auto insertion = LUT.try_emplace(
                member.name,
                ParameterInfo{
                    .byteSize = (uint16_t)member.padded_size,
                    // not sure what absolute offset is, but .offset seems to work
                    .byteOffsetInBuffer = (uint16_t)member.offset,
                });
            assert(insertion.second); // assertion must have happened, otherwise something is srsly wrong
        }

        for(int i = 0; i < VulkanRenderer::FRAMES_IN_FLIGHT; i++)
        {
            material->parameters.gpuBuffers[i] = createBuffer(Buffer::CreateInfo{
                .info =
                    {
                        .size = material->parameters.bufferSize,
                        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        .memoryAllocationInfo =
                            {
                                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                         VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            },
                    },
                .initialData = material->parameters.cpuBuffer});
        }
    }

    if(materialInstanceParametersBinding != nullptr)
    {
        // only create parameter LUT, actual buffers will be allocated when instances are created

        material->instanceParametersBufferSize = materialInstanceParametersBinding->block.padded_size;

        // Store offset (and size?) as LUT by name for writing parameters later
        material->instanceParametersLUT =
            new std::unordered_map<std::string, ParameterInfo, StringHash, std::equal_to<>>;
        auto& LUT = *material->instanceParametersLUT;

        std::span<SpvReflectBlockVariable> members{
            materialInstanceParametersBinding->block.members,
            materialInstanceParametersBinding->block.member_count};

        for(auto& member : members)
        {
            assert(member.padded_size >= member.size);

            auto insertion = LUT.try_emplace(
                member.name,
                ParameterInfo{
                    .byteSize = (uint16_t)member.padded_size,
                    // not sure what absolute offset is, but .offset seems to work
                    .byteOffsetInBuffer = (uint16_t)member.offset,
                });
            assert(insertion.second); // assertion must have happened, otherwise something is srsly wrong
        }
    }

    nameToMaterialLUT.insert({std::string{matName}, newMaterialHandle});
    // BREAKPOINT;

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

    VulkanRenderer& renderer = *VulkanRenderer::get();
    VkDevice device = renderer.device;
    const VmaAllocator* allocator = &renderer.allocator;

    if(material->parameters.cpuBuffer != nullptr)
    {
        ::free(material->parameters.cpuBuffer);

        uint32_t bufferCount = ArraySize(material->parameters.gpuBuffers);
        for(int i = 0; i < bufferCount; i++)
        {
            Handle<Buffer> bufferHandle = material->parameters.gpuBuffers[i];
            const Buffer* buffer = get(bufferHandle);
            if(buffer == nullptr)
            {
                return;
            }
            const VkBuffer vkBuffer = buffer->buffer;
            const VmaAllocation vmaAllocation = buffer->allocation;
            renderer.deleteQueue.pushBack([=]() { vmaDestroyBuffer(*allocator, vkBuffer, vmaAllocation); });
            bufferPool.remove(bufferHandle);
        }
    }

    delete material->parametersLUT;

    if(material->pipeline != VK_NULL_HANDLE)
    {
        renderer.deleteQueue.pushBack([=]() { vkDestroyPipeline(device, material->pipeline, nullptr); });
    }

    renderer.deleteQueue.pushBack(
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

    const VertexInputDescription vertexDescription = VertexInputDescription::getDefault();
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

    if(vkCreateGraphicsPipelines(
           renderer.device, renderer.pipelineCache, 1, &pipelineCrInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        std::cout << "Failed to create pipeline!" << std::endl;
        assert(false);
    }
}