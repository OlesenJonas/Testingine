#include "ResourceManager.hpp"
#include "Graphics/Buffer/Buffer.hpp"
#include "Graphics/Device/VulkanDevice.hpp"
#include "Graphics/Shaders/HLSL.hpp"
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
    auto clearMultiPool = [&](auto&& pool)
    {
        for(auto iter = pool.begin(); iter != pool.end(); iter++)
        {
            destroy(*iter);
        }
    };
    clearMultiPool(meshPool);
    clearMultiPool(materialInstancePool);
    clearMultiPool(materialPool);
    clearPool(computeShaderPool);
}

Buffer::Handle ResourceManager::createBuffer(Buffer::CreateInfo&& createInfo)
{
    return VulkanDevice::impl()->createBuffer(std::move(createInfo));
}

void ResourceManager::destroy(Buffer::Handle handle) { VulkanDevice::impl()->destroy(handle); }

Mesh::Handle ResourceManager::createMesh(const char* file, std::string name)
{
    std::string_view fileView{file};
    auto fileName = PathHelpers::fileName(fileView);
    auto fileExtension = PathHelpers::extension(fileView);

    std::string meshName = name.empty() ? std::string{fileName} : std::move(name);

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

    return createMesh(vertexPositions, vertexAttributes, {}, std::move(meshName));
}

Mesh::Handle ResourceManager::createMesh(
    Span<const Mesh::PositionType> vertexPositions,
    Span<const Mesh::VertexAttributes> vertexAttributes,
    Span<const uint32_t> indices,
    std::string name)
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

    Buffer::Handle positionBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (name + "_positionsBuffer"),
        .size = vertexPositions.size() * sizeof(glm::vec3),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::VertexBuffer | ResourceState::Storage | ResourceState::TransferDst,
        .initialState = ResourceState::VertexBuffer,
        .initialData = {(uint8_t*)vertexPositions.data(), vertexPositions.size() * sizeof(vertexPositions[0])},
    });

    Buffer::Handle attributesBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (name + "_attributesBuffer"),
        .size = vertexAttributes.size() * sizeof(vertexAttributes[0]),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::VertexBuffer | ResourceState::Storage | ResourceState::TransferDst,
        .initialState = ResourceState::VertexBuffer,
        .initialData = {(uint8_t*)vertexAttributes.data(), vertexAttributes.size() * sizeof(vertexAttributes[0])},
    });

    Buffer::Handle indexBufferHandle = createBuffer(Buffer::CreateInfo{
        .debugName = (name + "_indexBuffer"),
        .size = indices.size() * sizeof(indices[0]),
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::IndexBuffer | ResourceState::Storage | ResourceState::TransferDst,
        .initialState = ResourceState::IndexBuffer,
        .initialData = {(uint8_t*)indices.data(), indices.size() * sizeof(indices[0])},
    });

    Mesh::Handle newMeshHandle = meshPool.insert(
        name,
        Mesh::RenderData{
            .indexCount = uint32_t(indices.size()),
            .indexBuffer = indexBufferHandle,
            .positionBuffer = positionBufferHandle,
            .attributeBuffer = attributesBufferHandle,
            .gpuIndex = 0xFFFFFFFF,
        });

    nameToMeshLUT.insert({std::string{name}, newMeshHandle});

    return newMeshHandle;
}

void ResourceManager::destroy(Mesh::Handle handle)
{
    if(!meshPool.isHandleValid(handle))
        return;

    auto iter = nameToMeshLUT.find(*get<std::string>(handle));
    assert(iter != nameToMeshLUT.end());
    nameToMeshLUT.erase(iter);

    VulkanDevice* device = VulkanDevice::impl();
    Mesh::RenderData& renderData = *get<Mesh::RenderData>(handle);
    device->destroy(renderData.positionBuffer);
    device->destroy(renderData.attributeBuffer);
    device->destroy(renderData.indexBuffer);
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

void ResourceManager::destroy(Texture::Handle handle)
{
    auto iter = nameToTextureLUT.find(*get<std::string>(handle));
    assert(iter != nameToTextureLUT.end());
    nameToTextureLUT.erase(iter);
    VulkanDevice::impl()->destroy(handle);
}

Handle<TextureView> ResourceManager::createTextureView(TextureView::CreateInfo&& createInfo)
{
    return VulkanDevice::impl()->createTextureView(std::move(createInfo));
}

void ResourceManager::destroy(Handle<TextureView> handle) { VulkanDevice::impl()->destroy(handle); }

Material::Handle ResourceManager::createMaterial(Material::CreateInfo&& crInfo)
{
    std::string_view fileView{crInfo.fragmentShader.sourcePath};
    if(crInfo.debugName.empty())
    {
        crInfo.debugName = PathHelpers::fileName(fileView);
    }

    // todo: handle naming collisions
    auto iterator = nameToMaterialLUT.find(crInfo.debugName);
    assert(iterator == nameToMaterialLUT.end());

    std::vector<uint32_t> vertexBinary = compileHLSL(crInfo.vertexShader.sourcePath, Shaders::Stage::Vertex);
    std::vector<uint32_t> fragmentBinary = compileHLSL(crInfo.fragmentShader.sourcePath, Shaders::Stage::Fragment);

    // Parse Shader Interface -------------

    Shaders::Reflection::Module vertModule{vertexBinary};
    Shaders::Reflection::Module fragModule{fragmentBinary};

    auto [parametersMap, parametersBufferSize] = vertModule.parseMaterialParams();
    if(parametersBufferSize == 0)
        std::tie(parametersMap, parametersBufferSize) = fragModule.parseMaterialParams();

    auto [instanceParametersMap, instanceParametersBufferSize] = vertModule.parseMaterialInstanceParams();
    if(instanceParametersBufferSize == 0)
        std::tie(instanceParametersMap, instanceParametersBufferSize) = fragModule.parseMaterialInstanceParams();

    VkPipeline pipeline = VulkanDevice::impl()->createGraphicsPipeline(VulkanDevice::PipelineCreateInfo{
        .debugName = crInfo.debugName,
        .vertexSpirv = vertexBinary,
        .fragmentSpirv = fragmentBinary,
        .colorFormats = crInfo.colorFormats,
        .depthFormat = crInfo.depthFormat,
        .stencilFormat = crInfo.stencilFormat,
    });
    Material::ParameterMap parameterMap{.bufferSize = parametersBufferSize, .map = std::move(parametersMap)};
    Material::InstanceParameterMap instanceParameterMap{
        .bufferSize = instanceParametersBufferSize, .map = instanceParametersMap};
    Material::ParameterBuffer paramBuffer{
        .size = static_cast<uint32_t>(parametersBufferSize),
        .cpuBuffer = new uint8_t[parametersBufferSize],
        .deviceBuffer = parametersBufferSize == 0
                            ? Buffer::Handle::Null()
                            : createBuffer(Buffer::CreateInfo{
                                  .debugName = (std::string{crInfo.debugName} + "ParamsGPU"),
                                  .size = parametersBufferSize,
                                  .memoryType = Buffer::MemoryType::GPU,
                                  .allStates = ResourceState::UniformBuffer | ResourceState::TransferDst,
                                  .initialState = ResourceState::TransferDst,
                              }),
    };
    if(parametersBufferSize > 0)
    {
        memset(paramBuffer.cpuBuffer, 0, parametersBufferSize);
    }

    Material::Handle newMaterialHandle = materialPool.insert(
        std::string{crInfo.debugName},
        pipeline,
        std::move(parameterMap),
        std::move(instanceParameterMap),
        std::move(paramBuffer),
        parametersBufferSize > 0 // dirty
    );

    nameToMaterialLUT.insert({std::string{crInfo.debugName}, newMaterialHandle});

    return newMaterialHandle;
}

void ResourceManager::destroy(Material::Handle handle)
{
    if(!materialPool.isHandleValid(handle))
        return;

    auto iter = nameToMaterialLUT.find(*get<std::string>(handle));
    assert(iter != nameToMaterialLUT.end());
    nameToMaterialLUT.erase(iter);

    VulkanDevice* gfxDevice = VulkanDevice::impl();

    // safe to instantly delete here? could this still be in flight?
    //      API to add to device delete queue aswell?
    //      otherwise delete queue to this layer

    gfxDevice->destroy(*get<VkPipeline>(handle));

    auto* params = get<Material::ParameterBuffer>(handle);
    delete[] params->cpuBuffer;
    params->cpuBuffer = nullptr;
    destroy(params->deviceBuffer);

    materialPool.remove(handle);
}

bool ResourceManager::reloadMaterial(Material::Handle handle)
{
    // need to (queue) destruction of old members

    return true;
}

MaterialInstance::Handle ResourceManager::createMaterialInstance(Material::Handle parent)
{
    auto* instanceParameterMap = get<Material::InstanceParameterMap>(parent);
    auto bufferSize = instanceParameterMap->bufferSize;

    MaterialInstance::Handle newHandle = materialInstancePool.insert(
        *get<std::string>(parent) + "Instance", // TODO: better name
        parent,
        MaterialInstance::ParameterBuffer{
            .size = static_cast<uint32_t>(bufferSize),
            .cpuBuffer = new uint8_t[bufferSize],
            // Dont need to manage names for this, so could create directly through gfx device
            .deviceBuffer = bufferSize == 0
                                ? Buffer::Handle::Null()
                                : createBuffer(Buffer::CreateInfo{
                                      .debugName = *get<std::string>(parent) + "InstanceParamsGPU",
                                      .size = bufferSize,
                                      .memoryType = Buffer::MemoryType::GPU,
                                      .allStates = ResourceState::UniformBuffer | ResourceState::TransferDst,
                                      .initialState = ResourceState::TransferDst,
                                  }),
        },
        bufferSize > 0 // dirty
    );

    if(bufferSize > 0)
    {
        MaterialInstance::ParameterBuffer& params = *get<MaterialInstance::ParameterBuffer>(newHandle);
        memset(params.cpuBuffer, 0, params.size);
    }

    return newHandle;
}

void ResourceManager::destroy(MaterialInstance::Handle handle)
{
    if(!materialInstancePool.isHandleValid(handle))
        return;

    VulkanDevice* gfxDevice = VulkanDevice::impl();

    MaterialInstance::ParameterBuffer& params = *get<MaterialInstance::ParameterBuffer>(handle);
    // see note in destroy(Material::Handle handle)
    delete[] params.cpuBuffer;
    params.cpuBuffer = nullptr;
    destroy(params.deviceBuffer);

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

    std::vector<uint32_t> shaderBinary = compileHLSL(createInfo.sourcePath, Shaders::Stage::Compute);

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