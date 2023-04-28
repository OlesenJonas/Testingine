#pragma once

// todo: only forward declarations where possible
#include <intern/Datastructures/Pool.hpp>
#include <intern/Datastructures/Span.hpp>
#include <intern/Graphics/Buffer/Buffer.hpp>
#include <intern/Graphics/Material/Material.hpp>
#include <intern/Graphics/Mesh/Mesh.hpp>
#include <intern/Graphics/Texture/Texture.hpp>
#include <intern/Graphics/Texture/TextureView.hpp>
#include <intern/Misc/StringHash.hpp>
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
    void init();

    [[nodiscard]] static inline ResourceManager* get()
    {
        return ptr;
    }

    Handle<Buffer> createBuffer(Buffer::CreateInfo info, std::string_view name = "");

    Handle<Mesh> createMesh(const char* file, std::string_view name = "");
    Handle<Mesh> createMesh(Span<Vertex> vertices, std::string_view name);

    Handle<Texture> createTexture(const char* file, VkImageUsageFlags usage, std::string_view name = "");
    Handle<Texture> createTexture(Texture::Info info, std::string_view name);
    // Handle<Texture> createTextureView(Handle<Texture> texture, TextureView::Info info, std::string_view name);

    Handle<Material> createMaterial(Material::CreateInfo crInfo, std::string_view name = "");

    Handle<Sampler> createSampler(Sampler::Info&& info);

    // todo: rename all these to just free(Handle<...>)?
    void deleteBuffer(Handle<Buffer> handle);
    void deleteMesh(Handle<Mesh> handle);
    void deleteTexture(Handle<Texture> handle);
    // like here
    void free(Handle<Material> handle);
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
    // Samplers dont use pool, so special getter needed
    inline Sampler* get(Handle<Sampler> handle)
    {
        return &samplerArray[handle.index];
    }

    inline Handle<Mesh> getMesh(std::string_view name)
    {
        const auto iterator = nameToMeshLUT.find(name);
        return (iterator == nameToMeshLUT.end()) ? Handle<Mesh>{} : iterator->second;
    }
    inline Handle<Texture> getTexture(std::string_view name)
    {
        const auto iterator = nameToTextureLUT.find(name);
        return (iterator == nameToTextureLUT.end()) ? Handle<Texture>{} : iterator->second;
    }
    inline Handle<Material> getMaterial(std::string_view name)
    {
        const auto iterator = nameToMaterialLUT.find(name);
        return (iterator == nameToMaterialLUT.end()) ? Handle<Material>{} : iterator->second;
    }

    void cleanup();

    constexpr static uint32_t samplerLimit = 32;

  private:
    inline static ResourceManager* ptr = nullptr;

    // just using standard unordered_map here, because I dont want to think about yet another datastructure atm
    std::unordered_map<std::string, Handle<Mesh>, StringHash, std::equal_to<>> nameToMeshLUT;
    std::unordered_map<std::string, Handle<Texture>, StringHash, std::equal_to<>> nameToTextureLUT;
    std::unordered_map<std::string, Handle<Material>, StringHash, std::equal_to<>> nameToMaterialLUT;

    Pool<Buffer> bufferPool{10};
    Pool<Mesh> meshPool{10};
    Pool<Texture> texturePool{10};
    Pool<Material> materialPool{10};

    // this value needs to match the one in bindless.glsl!
    // todo: could pass this as a define to shader compilation I guess
    std::array<Sampler, samplerLimit> samplerArray;
    // since samplers are never deleted, a simple linear index is enough
    // todo: free entries after they arent in use for some time, then need mechanism to reclaim that entry
    uint32_t freeSamplerIndex = 0;
};