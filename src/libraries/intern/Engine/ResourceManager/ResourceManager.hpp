#pragma once

// todo: only forward declarations where possible
#include <Datastructures/Pool.hpp>
#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Buffer/Buffer.hpp>
#include <Engine/Graphics/Material/Material.hpp>
#include <Engine/Graphics/Material/MaterialInstance.hpp>
#include <Engine/Graphics/Mesh/Mesh.hpp>
#include <Engine/Graphics/Texture/Texture.hpp>
#include <Engine/Graphics/Texture/TextureView.hpp>
#include <Engine/Misc/StringHash.hpp>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

/*
    ResourceManager needs to ensure that the GPU representations of all the objects are properly created &
    destroyed
    The objects themselves should only hold CPU information and POD meta data
        //todo: that is probably violated in some structs, need to clean this and decide better what to put where!
*/
class ResourceManager
{
  public:
    CREATE_STATIC_GETTER(ResourceManager);

    void init();

    Handle<Buffer> createBuffer(Buffer::CreateInfo info, std::string_view name = "");

    Handle<Mesh> createMesh(const char* file, std::string_view name = "");
    // indices can be {}, but then a trivial index list will still be used!
    Handle<Mesh> createMesh(
        Span<glm::vec3> vertexPositions,
        Span<Mesh::VertexAttributes> vertexAttributes,
        Span<uint32_t> indices,
        std::string_view name);

    Handle<Texture> createTexture(const char* file, VkImageUsageFlags usage, std::string_view name = "");
    Handle<Texture> createTexture(Texture::Info info, std::string_view name);
    // Handle<Texture> createTextureView(Handle<Texture> texture, TextureView::Info info, std::string_view name);

    Handle<Material> createMaterial(Material::CreateInfo crInfo, std::string_view name = "");
    Handle<MaterialInstance> createMaterialInstance(Handle<Material> material);

    Handle<Sampler> createSampler(Sampler::Info&& info);

    // todo: track resource usage so no stuff thats in use gets deleted
    // todo: rename all these to just free(Handle<...>)?
    void deleteBuffer(Handle<Buffer> handle);
    void deleteMesh(Handle<Mesh> handle);
    void deleteTexture(Handle<Texture> handle);
    // like here
    void free(Handle<Material> handle);
    void free(Handle<MaterialInstance> handle);
    // no explicit free function for samplers, since they could be used in multiple places
    // and usages arent tracked yet!

    // Accessing -----------------

#define HANDLE_TO_PTR_GETTER(T, pool)                                                                             \
    inline T* get(Handle<T> handle)                                                                               \
    {                                                                                                             \
        return pool.get(handle);                                                                                  \
    }
    HANDLE_TO_PTR_GETTER(Buffer, bufferPool);
    HANDLE_TO_PTR_GETTER(Mesh, meshPool);
    HANDLE_TO_PTR_GETTER(Texture, texturePool);
    HANDLE_TO_PTR_GETTER(Material, materialPool);
    HANDLE_TO_PTR_GETTER(MaterialInstance, materialInstancePool);
    // Samplers dont use pool, so special getter needed
    inline Sampler* get(Handle<Sampler> handle)
    {
        return &samplerArray[handle.index];
    }

#define NAME_TO_HANDLE_GETTER(T, LUT)                                                                             \
    inline Handle<T> get##T(std::string_view name)                                                                \
    {                                                                                                             \
        const auto iterator = LUT.find(name);                                                                     \
        return (iterator == LUT.end()) ? Handle<T>{} : iterator->second;                                          \
    }
    NAME_TO_HANDLE_GETTER(Mesh, nameToMeshLUT);
    NAME_TO_HANDLE_GETTER(Texture, nameToTextureLUT);
    NAME_TO_HANDLE_GETTER(Material, nameToMaterialLUT);

    void cleanup();

    constexpr static uint32_t samplerLimit = 32;

  private:
    // just using standard unordered_map here, because I dont want to think about yet another datastructure atm
    std::unordered_map<std::string, Handle<Mesh>, StringHash, std::equal_to<>> nameToMeshLUT;
    std::unordered_map<std::string, Handle<Texture>, StringHash, std::equal_to<>> nameToTextureLUT;
    std::unordered_map<std::string, Handle<Material>, StringHash, std::equal_to<>> nameToMaterialLUT;

    Pool<Buffer> bufferPool{10};
    Pool<Mesh> meshPool{10};
    Pool<Texture> texturePool{10};
    Pool<Material> materialPool{10};
    Pool<MaterialInstance> materialInstancePool{10};

    // this value needs to match the one in bindless.glsl!
    // todo: could pass this as a define to shader compilation I guess
    std::array<Sampler, samplerLimit> samplerArray;
    // since samplers are never deleted, a simple linear index is enough
    // todo: free entries after they arent in use for some time, then need mechanism to reclaim that entry
    uint32_t freeSamplerIndex = 0;
};