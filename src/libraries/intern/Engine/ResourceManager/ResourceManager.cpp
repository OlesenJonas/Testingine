#include "ResourceManager.hpp"
#include "Graphics/Buffer/Buffer.hpp"
#include "Graphics/Device/VulkanDevice.hpp"
#include "Graphics/Shaders/GLSL.hpp"
#include "Graphics/Shaders/HLSL.hpp"
#include "SPIRV-Reflect/spirv_reflect.h"
#include <Datastructures/ArrayHelpers.hpp>
#include <Engine/Application/Application.hpp>
#include <Engine/Graphics/Material/Material.hpp>
#include <Engine/Misc/PathHelpers.hpp>
#include <TinyOBJ/tiny_obj_loader.h>
#include <span>
#include <vulkan/vulkan_core.h>

/*
    each "type implementation" is in the respective .cpp file
    ie: createBuffer in Buffer.cpp, createTexture in Texture.cpp ...
    //TODO: change?
*/

void ResourceManager::init()
{
    INIT_STATIC_GETTER();

    meshPool.init(10);
    materialPool.init(10);
    materialInstancePool.init(10);
    computeShaderPool.init(10);

    _initialized = true;
}

void ResourceManager::cleanup()
{
    auto clearPool = [&](auto&& pool)
    {
        for(auto iter = pool.begin(); iter != pool.end(); iter++)
        {
            destroy(iter.asHandle());
        }
    };
    clearPool(meshPool);
    clearPool(materialInstancePool);
    clearPool(materialPool);
    clearPool(computeShaderPool);
}

Handle<Buffer> ResourceManager::createBuffer(Buffer::CreateInfo&& createInfo)
{
    return VulkanDevice::get()->createBuffer(std::move(createInfo));
}

void ResourceManager::destroy(Handle<Buffer> handle) { VulkanDevice::get()->destroy(handle); }

Handle<Mesh> ResourceManager::createMesh(const char* file, std::string_view name)
{
    std::string_view fileView{file};
    auto lastDirSep = fileView.find_last_of("/\\");
    auto extensionStart = fileView.find_last_of('.');
    std::string_view meshName =
        name.empty() ? fileView.substr(lastDirSep + 1, extensionStart - lastDirSep - 1) : name;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file, nullptr);
    if(!warn.empty())
    {
        std::cout << "TinyOBJ WARN: " << warn << std::endl;
    }

    if(!err.empty())
    {
        std::cout << "TinyOBJ ERR: " << err << std::endl;
        return {};
    }

    std::vector<glm::vec3> vertexPositions;
    std::vector<Mesh::VertexAttributes> vertexAttributes;

    // TODO: **VERY** unoptimized, barebones
    for(size_t s = 0; s < shapes.size(); s++)
    {
        size_t indexOffset = 0;
        for(size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            int fv = 3;

            for(size_t v = 0; v < fv; v++)
            {
                tinyobj::index_t idx = shapes[s].mesh.indices[indexOffset + v];

                // position
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

                // normal
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

                // uv
                tinyobj::real_t uvx = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t uvy = attrib.texcoords[2 * idx.texcoord_index + 1];

                vertexPositions.emplace_back(vx, vy, vz);
                vertexAttributes.emplace_back(Mesh::VertexAttributes{
                    .normal = {nx, ny, nz},
                    .color = {nx, ny, nz},
                    .uv = {uvx, 1.0 - uvy},
                });
            }
            indexOffset += fv;
        }
    }

    return createMesh(vertexPositions, vertexAttributes, {}, meshName);
}

Handle<Mesh> ResourceManager::createMesh(
    Span<const Mesh::PositionType> vertexPositions,
    Span<const Mesh::VertexAttributes> vertexAttributes,
    Span<const uint32_t> indices,
    std::string_view name)
{
    assert(vertexAttributes.size() == vertexPositions.size());

    // todo: handle naming collisions
    auto iterator = nameToMeshLUT.find(name);
    assert(iterator == nameToMeshLUT.end());

    std::vector<uint32_t> trivialIndices{};
    if(indices.empty())
    {
        trivialIndices.resize(vertexAttributes.size());
        // dont want to pull <algorithm> for iota here
        for(int i = 0; i < trivialIndices.size(); i++)
        {
            trivialIndices[i] = i;
        }
        indices = trivialIndices;
    }

    Handle<Buffer> positionBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (std::string{name} + "_positionsBuffer"),
        .size = vertexPositions.size() * sizeof(glm::vec3),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::VertexBuffer | ResourceState::TransferDst,
        .initialState = ResourceState::VertexBuffer,
        .initialData = {(uint8_t*)vertexPositions.data(), vertexPositions.size() * sizeof(vertexPositions[0])},
    });

    Handle<Buffer> attributesBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (std::string{name} + "_attributesBuffer"),
        .size = vertexAttributes.size() * sizeof(vertexAttributes[0]),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::VertexBuffer | ResourceState::TransferDst,
        .initialState = ResourceState::VertexBuffer,
        .initialData = {(uint8_t*)vertexAttributes.data(), vertexAttributes.size() * sizeof(vertexAttributes[0])},
    });

    Handle<Buffer> indexBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (std::string{name} + "_indexBuffer"),
        .size = indices.size() * sizeof(indices[0]),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::IndexBuffer | ResourceState::TransferDst,
        .initialState = ResourceState::IndexBuffer,
        .initialData = {(uint8_t*)indices.data(), indices.size() * sizeof(indices[0])},
    });

    Handle<Mesh> newMeshHandle = meshPool.insert(Mesh{
        .name{name},
        .indexCount = uint32_t(indices.size()),
        .indexBuffer = indexBufferHandle,
        .positionBuffer = positionBufferHandle,
        .attributeBuffer = attributesBufferHandle});

    nameToMeshLUT.insert({std::string{name}, newMeshHandle});

    return newMeshHandle;
}

void ResourceManager::destroy(Handle<Mesh> handle)
{
    VulkanDevice* device = VulkanDevice::get();
    device->destroy(get(handle)->positionBuffer);
    device->destroy(get(handle)->attributeBuffer);
    device->destroy(get(handle)->indexBuffer);
    meshPool.remove(handle);
}

Handle<Sampler> ResourceManager::createSampler(Sampler::Info&& info)
{
    return VulkanDevice::get()->createSampler(info);
}

Handle<Texture> ResourceManager::createTexture(Texture::LoadInfo&& loadInfo)
{
    std::string extension{PathHelpers::extension(loadInfo.path)};

    std::string_view texName =
        loadInfo.debugName.empty() ? PathHelpers::fileName(loadInfo.path) : loadInfo.debugName;
    loadInfo.debugName = texName;

    Texture::CreateInfo createInfo;
    std::function<void()> cleanupFunc;
    if(extension == ".hdr")
    {
        std::tie(createInfo, cleanupFunc) = Texture::loadHDR(std::move(loadInfo));
    }
    else
    {
        std::tie(createInfo, cleanupFunc) = Texture::loadDefault(std::move(loadInfo));
    }

    Handle<Texture> newTextureHandle = createTexture(std::move(createInfo));

    cleanupFunc();

    return newTextureHandle;
}

Handle<Texture> ResourceManager::createTexture(Texture::CreateInfo&& createInfo)
{
    // todo: handle naming collisions, also handle case where debugname is empty?
    auto iterator = nameToMeshLUT.find(createInfo.debugName);
    assert(iterator == nameToMeshLUT.end());

    std::string nameCpy = createInfo.debugName;

    auto newHandle = VulkanDevice::get()->createTexture(std::move(createInfo));

    nameToTextureLUT.insert({std::string{nameCpy}, newHandle});

    return newHandle;
}

void ResourceManager::destroy(Handle<Texture> handle) { VulkanDevice::get()->destroy(handle); }

Handle<Material> ResourceManager::createMaterial(Material::CreateInfo&& crInfo)
{
    std::string_view fileView{crInfo.fragmentShader.sourcePath};
    if(crInfo.debugName.empty())
    {
        crInfo.debugName = PathHelpers::fileName(fileView);
    }

    // todo: handle naming collisions
    auto iterator = nameToMaterialLUT.find(crInfo.debugName);
    assert(iterator == nameToMaterialLUT.end());

    Handle<Material> newMaterialHandle = materialPool.insert();
    Material* material = get(newMaterialHandle);

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
                .debugName = (std::string{crInfo.debugName} + "Params" + std::to_string(i)),
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

    nameToMaterialLUT.insert({std::string{crInfo.debugName}, newMaterialHandle});

    material->pipeline =
        VulkanDevice::get()->createGraphicsPipeline(vertexBinary, fragmentBinary, crInfo.debugName);

    return newMaterialHandle;
}

void ResourceManager::destroy(Handle<Material> handle)
{
    Material* material = materialPool.get(handle);
    if(material == nullptr)
    {
        return;
    }

    VulkanDevice* gfxDevice = VulkanDevice::get();

    if(material->parameters.cpuBuffer != nullptr)
    {
        ::free(material->parameters.cpuBuffer);

        uint32_t bufferCount = ArraySize(material->parameters.gpuBuffers);
        for(int i = 0; i < bufferCount; i++)
        {
            Handle<Buffer> buffer = material->parameters.gpuBuffers[i];
            gfxDevice->destroy(buffer);
        }
    }

    delete material->parametersLUT;

    gfxDevice->destroy(material->pipeline);

    materialPool.remove(handle);
}

Handle<MaterialInstance> ResourceManager::createMaterialInstance(Handle<Material> material)
{
    Handle<MaterialInstance> matInstHandle = materialInstancePool.insert();
    MaterialInstance* matInst = get(matInstHandle);
    Material* parentMat = get(material);

    matInst->parentMaterial = material;
    matInst->parameters.bufferSize = parentMat->instanceParametersBufferSize;
    if(matInst->parameters.bufferSize > 0)
    {
        matInst->parameters.lut = parentMat->instanceParametersLUT;
        matInst->parameters.cpuBuffer = (char*)malloc(parentMat->instanceParametersBufferSize);
        memset(matInst->parameters.cpuBuffer, 0, matInst->parameters.bufferSize);

        for(int i = 0; i < VulkanDevice::FRAMES_IN_FLIGHT; i++)
        {
            matInst->parameters.gpuBuffers[i] = createBuffer(Buffer::CreateInfo{
                .size = matInst->parameters.bufferSize,
                .memoryType = Buffer::MemoryType::GPU_BUT_CPU_VISIBLE,
                .allStates = ResourceState::UniformBuffer | ResourceState::TransferDst,
                .initialState = ResourceState::UniformBuffer,
                .initialData = {(uint8_t*)matInst->parameters.cpuBuffer, matInst->parameters.bufferSize},
            });
        }
    }

    return matInstHandle;
}

void ResourceManager::destroy(Handle<MaterialInstance> handle)
{
    const MaterialInstance* matInst = materialInstancePool.get(handle);
    if(matInst == nullptr)
    {
        return;
    }

    VulkanDevice& gfxDevice = *VulkanDevice::get();
    VkDevice device = gfxDevice.device;
    const VmaAllocator* allocator = &gfxDevice.allocator;

    if(matInst->parameters.cpuBuffer != nullptr)
    {
        ::free(matInst->parameters.cpuBuffer);

        uint32_t bufferCount = ArraySize(matInst->parameters.gpuBuffers);
        for(int i = 0; i < bufferCount; i++)
        {
            Handle<Buffer> buffer = matInst->parameters.gpuBuffers[i];
            gfxDevice.destroy(buffer);
        }
    }

    materialInstancePool.remove(handle);
}

Handle<ComputeShader>
ResourceManager::createComputeShader(Shaders::StageCreateInfo&& createInfo, std::string_view debugName)
{
    std::string_view fileView{createInfo.sourcePath};
    if(debugName.empty())
    {
        debugName = PathHelpers::fileName(fileView);
    }

    // todo: handle naming collisions
    auto iterator = nameToMaterialLUT.find(debugName);
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

void ResourceManager::destroy(Handle<ComputeShader> handle)
{
    ComputeShader* compShader = computeShaderPool.get(handle);
    if(compShader == nullptr)
    {
        return;
    }

    VulkanDevice* gfxDevice = VulkanDevice::get();
    gfxDevice->destroy(compShader->pipeline);

    computeShaderPool.remove(handle);
}