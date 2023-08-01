#include "ComputeShader.hpp"

#include <Datastructures/Pool.hpp>
#include <Graphics/Renderer/VulkanDebug.hpp>
#include <Graphics/Shaders/GLSL.hpp>
#include <Graphics/Shaders/HLSL.hpp>
#include <ResourceManager/ResourceManager.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <iostream>
#include <shaderc/shaderc.h>
#include <span>
#include <string_view>

Handle<ComputeShader>
ResourceManager::createComputeShader(Shaders::StageCreateInfo&& createInfo, std::string_view debugName)
{
    std::string_view fileView{createInfo.sourcePath};
    auto lastDirSep = fileView.find_last_of("/\\");
    auto extensionStart = fileView.find_last_of('.');
    std::string_view shaderName =
        debugName.empty() ? fileView.substr(lastDirSep + 1, extensionStart - lastDirSep - 1) : debugName;

    // todo: handle naming collisions
    auto iterator = nameToMaterialLUT.find(shaderName);
    assert(iterator == nameToMaterialLUT.end());

    Handle<ComputeShader> newComputeShaderHandle = computeShaderPool.insert();
    ComputeShader* computeShader = get(newComputeShaderHandle);

    VulkanRenderer& renderer = *VulkanRenderer::get();

    std::vector<uint32_t> shaderBinary;

    if(createInfo.sourceLanguage == Shaders::Language::HLSL)
    {
        shaderBinary = compileHLSL(createInfo.sourcePath, Shaders::Stage::Compute);
    }
    else
    {
        shaderBinary = compileGLSL(createInfo.sourcePath, Shaders::Stage::Compute);
    }

    // Shader Modules ----------------
    VkShaderModuleCreateInfo vertSMcrInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = 4u * shaderBinary.size(), // size in bytes, but spirvBinary is uint32 vector!
        .pCode = shaderBinary.data(),
    };
    if(vkCreateShaderModule(renderer.device, &vertSMcrInfo, nullptr, &computeShader->shaderModule) != VK_SUCCESS)
    {
        assert(false);
    }
    setDebugName(computeShader->shaderModule, (std::string{shaderName} + "module").c_str());

    // Parse Shader Interface -------------

    // todo: wrap into function taking shader byte code(s) (span)? could be moved into Shaders.cpp
    //      was used only in fragment and vertex shader compilation, but its used here now aswell
    //      so maybe its time to factor out

    SpvReflectShaderModule reflModule;
    SpvReflectResult result;
    result = spvReflectCreateShaderModule(shaderBinary.size() * 4, shaderBinary.data(), &reflModule);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::span<SpvReflectDescriptorBinding> vertDescriptorBindings{
        reflModule.descriptor_bindings, reflModule.descriptor_binding_count};

    // nameToMaterialLUT.insert({std::string{matName}, newMaterialHandle});

    computeShader->createPipeline();
    setDebugName(computeShader->pipeline, (std::string{shaderName}).c_str());

    return newComputeShaderHandle;
}

void ResourceManager::free(Handle<ComputeShader> handle)
{
    const ComputeShader* compShader = computeShaderPool.get(handle);
    if(compShader == nullptr)
    {
        return;
    }

    VulkanRenderer& renderer = *VulkanRenderer::get();
    VkDevice device = renderer.device;
    const VmaAllocator* allocator = &renderer.allocator;

    if(compShader->pipeline != VK_NULL_HANDLE)
    {
        renderer.deleteQueue.pushBack([=]() { vkDestroyPipeline(device, compShader->pipeline, nullptr); });
    }

    renderer.deleteQueue.pushBack([=]() { vkDestroyShaderModule(device, compShader->shaderModule, nullptr); });

    computeShaderPool.remove(handle);
}

void ComputeShader::createPipeline()
{
    // todo: check if pipeline with given parameters already exists, and just return that in case it does!

    VulkanRenderer& renderer = *VulkanRenderer::get();

    // Creating the pipeline ---------------------------

    const VkPipelineShaderStageCreateInfo shaderStageCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };

    const VkComputePipelineCreateInfo pipelineCrInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shaderStageCrInfo,
        .layout = renderer.bindlessPipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    if(vkCreateComputePipelines(renderer.device, renderer.pipelineCache, 1, &pipelineCrInfo, nullptr, &pipeline) !=
       VK_SUCCESS)
    {
        std::cout << "Failed to create pipeline!" << std::endl;
        assert(false);
    }
}