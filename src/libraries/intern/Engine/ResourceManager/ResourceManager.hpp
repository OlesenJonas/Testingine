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

#define CREATE_NAME_TO_HANDLE_GETTER(T, LUT)                                                                      \
    inline Handle<T> get##T(std::string_view name)                                                                \
    {                                                                                                             \
        const auto iterator = LUT.find(name);                                                                     \
        return (iterator == LUT.end()) ? Handle<T>::Null() : iterator->second;                                    \
    }

  public:
    void init();

    // todo: track resource usage so no stuff thats in use gets deleted

    Handle<Buffer> createBuffer(Buffer::CreateInfo&& createInfo);
    void destroy(Handle<Buffer> handle);
    inline Buffer* get(Handle<Buffer> handle) { return VulkanDevice::get()->get(handle); }

    Handle<Mesh> createMesh(const char* file, std::string_view name = "");
    // indices can be {}, but then a trivial index list will still be used!
    Handle<Mesh> createMesh(
        Span<const Mesh::PositionType> vertexPositions,
        Span<const Mesh::VertexAttributes> vertexAttributes,
        Span<const uint32_t> indices,
        std::string_view name);
    void destroy(Handle<Mesh> handle);
    inline Mesh* get(Handle<Mesh> handle) { return meshPool.get(handle); }
    CREATE_NAME_TO_HANDLE_GETTER(Mesh, nameToMeshLUT);

    Handle<Sampler> createSampler(Sampler::Info&& info);
    inline Sampler* get(Handle<Sampler> handle) { return VulkanDevice::get()->get(handle); };

    Handle<Texture> createTexture(Texture::CreateInfo&& createInfo);
    Handle<Texture> createTexture(Texture::LoadInfo&& loadInfo);
    void destroy(Handle<Texture> handle);
    inline Texture* get(Handle<Texture> handle) { return VulkanDevice::get()->get(handle); };
    CREATE_NAME_TO_HANDLE_GETTER(Texture, nameToTextureLUT);

    Handle<TextureView> createTextureView(TextureView::CreateInfo&& createInfo);
    void destroy(Handle<TextureView> handle);
    inline TextureView* get(Handle<TextureView> handle) { return VulkanDevice::get()->get(handle); };

    Handle<Material> createMaterial(Material::CreateInfo&& crInfo);
    void destroy(Handle<Material> handle);
    inline Material* get(Handle<Material> handle) { return materialPool.get(handle); };
    CREATE_NAME_TO_HANDLE_GETTER(Material, nameToMaterialLUT);

    Handle<MaterialInstance> createMaterialInstance(Handle<Material> parent);
    void destroy(Handle<MaterialInstance> handle);
    inline MaterialInstance* get(Handle<MaterialInstance> handle) { return materialInstancePool.get(handle); };

    Handle<ComputeShader> createComputeShader(Shaders::StageCreateInfo&& createInfo, std::string_view debugName);
    void destroy(Handle<ComputeShader> handle);
    inline ComputeShader* get(Handle<ComputeShader> handle) { return computeShaderPool.get(handle); };

    // Accessing -----------------

    void cleanup();

    auto& getMaterialPool() { return materialPool; }
    auto& getMaterialInstancePool() { return materialInstancePool; }

  private:
    bool _initialized = false;

    Pool<Mesh> meshPool;
    Pool<Material> materialPool;
    Pool<MaterialInstance> materialInstancePool;
    Pool<ComputeShader> computeShaderPool;

    // just using standard unordered_map here, because I dont want to think about yet another datastructure atm
    std::unordered_map<std::string, Handle<Mesh>, StringHash, std::equal_to<>> nameToMeshLUT;
    std::unordered_map<std::string, Handle<Texture>, StringHash, std::equal_to<>> nameToTextureLUT;
    std::unordered_map<std::string, Handle<Material>, StringHash, std::equal_to<>> nameToMaterialLUT;
};

#undef HANDLE_TO_PTR_GETTER
#undef NAME_TO_HANDLE_GETTER
