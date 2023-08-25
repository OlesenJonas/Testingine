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
    return VulkanDevice::impl()->createBuffer(std::move(createInfo));
}

void ResourceManager::destroy(Handle<Buffer> handle) { VulkanDevice::impl()->destroy(handle); }

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
    VulkanDevice* device = VulkanDevice::impl();
    device->destroy(get(handle)->positionBuffer);
    device->destroy(get(handle)->attributeBuffer);
    device->destroy(get(handle)->indexBuffer);
    meshPool.remove(handle);
}

Handle<Sampler> ResourceManager::createSampler(Sampler::Info&& info)
{
    return VulkanDevice::impl()->createSampler(info);
}

Texture::Handle ResourceManager::createTexture(Texture::LoadInfo&& loadInfo)
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

    Texture::Handle newTextureHandle = createTexture(std::move(createInfo));

    cleanupFunc();

    return newTextureHandle;
}

Texture::Handle ResourceManager::createTexture(Texture::CreateInfo&& createInfo)
{
    // todo: handle naming collisions, also handle case where debugname is empty?
    auto iterator = nameToMeshLUT.find(createInfo.debugName);
    assert(iterator == nameToMeshLUT.end());

    std::string nameCpy = createInfo.debugName;

    auto newHandle = VulkanDevice::impl()->createTexture(std::move(createInfo));

    nameToTextureLUT.insert({std::string{nameCpy}, newHandle});

    return newHandle;
}

void ResourceManager::destroy(Texture::Handle handle) { VulkanDevice::impl()->destroy(handle); }

Handle<TextureView> ResourceManager::createTextureView(TextureView::CreateInfo&& createInfo)
{
    return VulkanDevice::impl()->createTextureView(std::move(createInfo));
}

void ResourceManager::destroy(Handle<TextureView> handle) { VulkanDevice::impl()->destroy(handle); }

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

    Material::ParameterMap parametersMap;
    size_t parametersBufferSize = 0;
    Material::ParameterMap instanceParametersMap;
    size_t instanceParametersBufferSize = 0;

    if(materialParametersBinding != nullptr)
    {
        parametersBufferSize = materialParametersBinding->block.padded_size;

        std::span<SpvReflectBlockVariable> members{
            materialParametersBinding->block.members, materialParametersBinding->block.member_count};

        for(auto& member : members)
        {
            assert(member.padded_size >= member.size);

            auto insertion = parametersMap.try_emplace(
                member.name,
                Material::ParameterInfo{
                    .byteSize = (uint16_t)member.padded_size,
                    // not sure what absolute offset is, but .offset seems to work
                    .byteOffsetInBuffer = (uint16_t)member.offset,
                });
            assert(insertion.second); // assertion must have happened, otherwise something is srsly wrong
        }
    }

    if(materialInstanceParametersBinding != nullptr)
    {
        instanceParametersBufferSize = materialInstanceParametersBinding->block.padded_size;

        std::span<SpvReflectBlockVariable> members{
            materialInstanceParametersBinding->block.members,
            materialInstanceParametersBinding->block.member_count};

        for(auto& member : members)
        {
            assert(member.padded_size >= member.size);

            auto insertion = instanceParametersMap.try_emplace(
                member.name,
                Material::ParameterInfo{
                    .byteSize = (uint16_t)member.padded_size,
                    // not sure what absolute offset is, but .offset seems to work
                    .byteOffsetInBuffer = (uint16_t)member.offset,
                });
            assert(insertion.second); // assertion must have happened, otherwise something is srsly wrong
        }
    }

    Handle<Material> newMaterialHandle = materialPool.insert(Material{
        .name = std::string{crInfo.debugName},
        .parametersLUT = std::move(parametersMap),
        .parametersBufferSize = parametersBufferSize,
        .instanceParametersLUT = std::move(instanceParametersMap),
        .instanceParametersBufferSize = instanceParametersBufferSize,
        .parameters =
            ParameterBuffer{
                .bufferSize = uint32_t(parametersBufferSize),
                .writeBuffer = new uint8_t[parametersBufferSize],
                // Dont need to manage names for this, so could create directly through gfx device
                .deviceBuffer = parametersBufferSize == 0
                                    ? Handle<Buffer>::Null()
                                    : createBuffer(Buffer::CreateInfo{
                                          .debugName = (std::string{crInfo.debugName} + "ParamsGPU"),
                                          .size = parametersBufferSize,
                                          .memoryType = Buffer::MemoryType::GPU,
                                          .allStates = ResourceState::UniformBuffer | ResourceState::TransferDst,
                                          .initialState = ResourceState::TransferDst,
                                      }),
            },
        .pipeline = VulkanDevice::impl()->createGraphicsPipeline(vertexBinary, fragmentBinary, crInfo.debugName),
        .dirty = parametersBufferSize > 0,
    });

    if(parametersBufferSize > 0)
    {
        Material* material = get(newMaterialHandle);
        memset(material->parameters.writeBuffer, 0, parametersBufferSize);
    }

    nameToMaterialLUT.insert({std::string{crInfo.debugName}, newMaterialHandle});

    return newMaterialHandle;
}

void ResourceManager::destroy(Handle<Material> handle)
{
    Material* material = materialPool.get(handle);
    if(material == nullptr)
    {
        return;
    }

    VulkanDevice* gfxDevice = VulkanDevice::impl();

    // safe to instantly delete here? could this still be in flight?
    //      API to add to device delete queue aswell?
    //      otherwise delete queue to this layer
    delete[] material->parameters.writeBuffer;
    material->parameters.writeBuffer = nullptr;
    destroy(material->parameters.deviceBuffer);
    gfxDevice->destroy(material->pipeline);

    materialPool.remove(handle);
}

Handle<MaterialInstance> ResourceManager::createMaterialInstance(Handle<Material> parent)
{
    Material* material = get(parent);
    auto parametersBufferSize = material->instanceParametersBufferSize;
    Handle<MaterialInstance> newHandle = materialInstancePool.insert(MaterialInstance{
        .parentMaterial = parent,
        .parameters =
            ParameterBuffer{
                .bufferSize = uint32_t(parametersBufferSize),
                .writeBuffer = new uint8_t[parametersBufferSize],
                // Dont need to manage names for this, so could create directly through gfx device
                .deviceBuffer = parametersBufferSize == 0
                                    ? Handle<Buffer>::Null()
                                    : createBuffer(Buffer::CreateInfo{
                                          .debugName = (material->name + "InstanceParamsGPU"),
                                          .size = parametersBufferSize,
                                          .memoryType = Buffer::MemoryType::GPU,
                                          .allStates = ResourceState::UniformBuffer | ResourceState::TransferDst,
                                          .initialState = ResourceState::TransferDst,
                                      }),
            },
        .dirty = parametersBufferSize > 0,
    });

    if(parametersBufferSize > 0)
    {
        MaterialInstance* instance = get(newHandle);
        memset(instance->parameters.writeBuffer, 0, parametersBufferSize);
    }

    return newHandle;
}

void ResourceManager::destroy(Handle<MaterialInstance> handle)
{
    MaterialInstance* matInst = materialInstancePool.get(handle);
    if(matInst == nullptr)
    {
        return;
    }

    VulkanDevice* gfxDevice = VulkanDevice::impl();

    // see note in destroy(Handle<Material> handle)
    delete[] matInst->parameters.writeBuffer;
    matInst->parameters.writeBuffer = nullptr;
    destroy(matInst->parameters.deviceBuffer);

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

    VulkanDevice& gfxDevice = *VulkanDevice::impl();

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

    VulkanDevice* gfxDevice = VulkanDevice::impl();
    gfxDevice->destroy(compShader->pipeline);

    computeShaderPool.remove(handle);
}