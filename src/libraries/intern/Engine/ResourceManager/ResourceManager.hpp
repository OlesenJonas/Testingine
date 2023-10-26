#pragma once

#include <Datastructures/Pool/Pool.hpp>
#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Buffer/Buffer.hpp>
#include <Engine/Graphics/Compute/ComputeShader.hpp>
#include <Engine/Graphics/Device/VulkanDevice.hpp>
#include <Engine/Graphics/Material/Material.hpp>
#include <Engine/Graphics/Mesh/Mesh.hpp>
#include <Engine/Graphics/Texture/Sampler.hpp>
#include <Engine/Graphics/Texture/Texture.hpp>
#include <Engine/Graphics/Texture/TextureView.hpp>
#include <Engine/Misc/Macros.hpp>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

/*
    responsible for managing resources on a higher level, like mapping names to handles
*/

class ResourceManager
{
    CREATE_STATIC_GETTER(ResourceManager);

#define CREATE_NAME_TO_MULTI_HANDLE_GETTER(T, LUT)                                                                \
    inline T::Handle get##T(std::string_view name)                                                                \
    {                                                                                                             \
        const auto iterator = LUT.find(name);                                                                     \
        return (iterator == LUT.end()) ? T::Handle::Null() : iterator->second;                                    \
    }

  public:
    void init();

    // todo: track resource usage so no stuff thats in use gets deleted

    // --------- Buffer -----------------------------------

    Buffer::Handle createBuffer(Buffer::CreateInfo&& createInfo);
    void destroy(Buffer::Handle handle);
    template <typename T>
    inline T* get(Buffer::Handle handle)
    {
        return VulkanDevice::impl()->get<T>(handle);
    }

    // --------- Mesh -----------------------------------

    Mesh::Handle createMesh(const char* file, std::string name = "");
    // indices can be {}, but then a trivial index list will still be used!
    Mesh::Handle createMesh(
        Span<const Mesh::PositionType> vertexPositions,
        Span<const Mesh::VertexAttributes> vertexAttributes,
        Span<const uint32_t> indices,
        std::string name);
    void destroy(Mesh::Handle handle);
    template <typename T>
        requires Mesh::Handle::holdsType<T>
    inline T* get(Mesh::Handle handle)
    {
        return meshPool.get<T>(handle);
    }
    CREATE_NAME_TO_MULTI_HANDLE_GETTER(Mesh, nameToMeshLUT);

    // --------- Sampler -----------------------------------

    Handle<Sampler> createSampler(Sampler::Info&& info);
    inline Sampler* get(Handle<Sampler> handle) { return VulkanDevice::impl()->get(handle); };

    // --------- Texture -----------------------------------

    Texture::Handle createTexture(Texture::CreateInfo&& createInfo);
    Texture::Handle createTexture(Texture::LoadInfo&& loadInfo);
    void destroy(Texture::Handle handle);
    template <typename T>
    T* get(Texture::Handle handle)
    {
        return VulkanDevice::impl()->get<T>(handle);
    };
    CREATE_NAME_TO_MULTI_HANDLE_GETTER(Texture, nameToTextureLUT);

    // --------- Texture View -----------------------------------

    Handle<TextureView> createTextureView(TextureView::CreateInfo&& createInfo);
    void destroy(Handle<TextureView> handle);
    inline TextureView* get(Handle<TextureView> handle) { return VulkanDevice::impl()->get(handle); };

    // --------- Material -----------------------------------

    Material::Handle createMaterial(Material::CreateInfo&& crInfo);
    void destroy(Material::Handle handle);
    template <typename T>
    T* get(Material::Handle handle)
    {
        return materialPool.get<T>(handle);
    };
    CREATE_NAME_TO_MULTI_HANDLE_GETTER(Material, nameToMaterialLUT);

    // --------- Material Instance -----------------------------------

    MaterialInstance::Handle createMaterialInstance(Material::Handle parent);
    void destroy(MaterialInstance::Handle handle);
    template <typename T>
    T* get(MaterialInstance::Handle handle)
    {
        return materialInstancePool.get<T>(handle);
    };

    // --------- Compute Shader -----------------------------------

    Handle<ComputeShader> createComputeShader(Shaders::StageCreateInfo&& createInfo, std::string_view debugName);
    void destroy(Handle<ComputeShader> handle);
    inline ComputeShader* get(Handle<ComputeShader> handle) { return computeShaderPool.get(handle); };

    // Accessing -----------------

    void cleanup();

    auto& getMaterialPool() { return materialPool; }
    auto& getMaterialInstancePool() { return materialInstancePool; }
    const auto& getMeshPool() { return meshPool; }

  private:
    bool _initialized = false;

    MultiPoolFromHandle<Mesh::Handle> meshPool;
    MultiPoolFromHandle<Material::Handle> materialPool;
    MultiPoolFromHandle<MaterialInstance::Handle> materialInstancePool;
    Pool<ComputeShader> computeShaderPool;

    // just using standard unordered_map here, because I dont want to think about yet another datastructure atm
    std::unordered_map<std::string, Mesh::Handle, StringHash, std::equal_to<>> nameToMeshLUT;
    std::unordered_map<std::string, Texture::Handle, StringHash, std::equal_to<>> nameToTextureLUT;
    std::unordered_map<std::string, Material::Handle, StringHash, std::equal_to<>> nameToMaterialLUT;
};

#undef HANDLE_TO_PTR_GETTER
#undef NAME_TO_HANDLE_GETTER
