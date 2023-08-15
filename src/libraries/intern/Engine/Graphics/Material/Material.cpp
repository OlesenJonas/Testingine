#include "Material.hpp"
#include <Datastructures/ArrayHelpers.hpp>
#include <Engine/Graphics/Device/VulkanConversions.hpp>
#include <Engine/Graphics/Device/VulkanDevice.hpp>
#include <Engine/Graphics/Shaders/GLSL.hpp>
#include <Engine/Graphics/Shaders/HLSL.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <Graphics/Shaders/Shaders.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <iostream>
#include <shaderc/shaderc.h>
#include <span> //TODO: replace with my span, but need natvis for debugging
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

    VulkanDevice& gfxDevice = *VulkanDevice::get();

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

        for(int i = 0; i < VulkanDevice::FRAMES_IN_FLIGHT; i++)
        {
            material->parameters.gpuBuffers[i] = createBuffer(Buffer::CreateInfo{
                .debugName = (std::string{matName} + "Params" + std::to_string(i)),
                .size = material->parameters.bufferSize,
                .memoryType = Buffer::MemoryType::GPU_BUT_CPU_VISIBLE,
                .allStates = ResourceState::UniformBuffer | ResourceState::TransferDst,
                .initialState = ResourceState::UniformBuffer,
                .initialData = {(uint8_t*)material->parameters.cpuBuffer, material->parametersBufferSize},
            });
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

    material->pipeline = gfxDevice.createGraphicsPipeline(vertexBinary, fragmentBinary, matName);

    return newMaterialHandle;
}

void ResourceManager::free(Handle<Material> handle)
{
    Material* material = materialPool.get(handle);
    if(material == nullptr)
    {
        return;
    }

    VulkanDevice& gfxDevice = *VulkanDevice::get();
    VkDevice device = gfxDevice.device;
    const VmaAllocator* allocator = &gfxDevice.allocator;

    if(material->parameters.cpuBuffer != nullptr)
    {
        ::free(material->parameters.cpuBuffer);

        uint32_t bufferCount = ArraySize(material->parameters.gpuBuffers);
        for(int i = 0; i < bufferCount; i++)
        {
            Handle<Buffer> bufferHandle = material->parameters.gpuBuffers[i];
            Buffer* buffer = get(bufferHandle);
            if(buffer == nullptr)
            {
                return;
            }
            gfxDevice.deleteBuffer(buffer);
            bufferPool.remove(bufferHandle);
        }
    }

    delete material->parametersLUT;

    gfxDevice.deleteGraphicsPipeline(material);

    materialPool.remove(handle);
}