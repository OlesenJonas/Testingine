#include "ComputeShader.hpp"

#include <Datastructures/Pool/Pool.hpp>
#include <Graphics/Device/VulkanDevice.hpp>
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

    VulkanDevice& gfxDevice = *VulkanDevice::get();

    std::vector<uint32_t> shaderBinary;

    if(createInfo.sourceLanguage == Shaders::Language::HLSL)
    {
        shaderBinary = compileHLSL(createInfo.sourcePath, Shaders::Stage::Compute);
    }
    else
    {
        shaderBinary = compileGLSL(createInfo.sourcePath, Shaders::Stage::Compute);
    }

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

    computeShader->pipeline = gfxDevice.createComputePipeline(shaderBinary, debugName);

    return newComputeShaderHandle;
}

void ResourceManager::free(Handle<ComputeShader> handle)
{
    ComputeShader* compShader = computeShaderPool.get(handle);
    if(compShader == nullptr)
    {
        return;
    }

    VulkanDevice* gfxDevice = VulkanDevice::get();
    gfxDevice->deleteComputePipeline(compShader);

    computeShaderPool.remove(handle);
}