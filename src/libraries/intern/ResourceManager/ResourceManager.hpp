#pragma once

#include <intern/Datastructures/Pool.hpp>
#include <intern/Datastructures/Span.hpp>
#include <intern/Graphics/Buffer/Buffer.hpp>
#include <intern/Graphics/Material/Material.hpp>
#include <intern/Graphics/Mesh/Mesh.hpp>
#include <intern/Graphics/Texture/Texture.hpp>
#include <intern/Graphics/Texture/TextureView.hpp>
#include <intern/Misc/StringHash.hpp>
#include <unordered_map>

/*
    ResourceManager needs to ensure that the GPU representations of all the objects are properly created &
   destroyed The objects themselves only hold CPU information and meta data
*/
class ResourceManager
{
  public:
    Handle<Buffer> createBuffer(Buffer::CreateInfo info);

    Handle<Mesh> createMesh(const char* file, std::string_view name = "");
    Handle<Mesh> createMesh(Span<Vertex> vertices, std::string_view name);

    Handle<Texture> createTexture(const char* file, VkImageUsageFlags usage, std::string_view name = "");
    Handle<Texture> createTexture(Texture::Info info, std::string_view name);
    // Handle<Texture> createTextureView(Handle<Texture> texture, TextureView::Info info, std::string_view name);

    // todo: only takes material by value atm because theres nothing really stored atm
    Handle<Material> createMaterial(Material mat, std::string_view name);

    // todo: rename all these to just delete(Handle<...>)?
    void deleteBuffer(Handle<Buffer> handle);
    void deleteMesh(Handle<Mesh> handle);
    void deleteTexture(Handle<Texture> handle);

    inline Buffer* get(Handle<Buffer> handle)
    {
        return bufferPool.get(handle);
    }
    inline Mesh* get(Handle<Mesh> handle)
    {
        return meshPool.get(handle);
    }
    inline Texture* get(Handle<Texture> handle)
    {
        return texturePool.get(handle);
    }
    inline Material* get(Handle<Material> handle)
    {
        return materialPool.get(handle);
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

  private:
    // just using standard unordered_map here, because I dont want to think about yet another datastructure atm
    std::unordered_map<std::string, Handle<Mesh>, StringHash, std::equal_to<>> nameToMeshLUT;
    std::unordered_map<std::string, Handle<Texture>, StringHash, std::equal_to<>> nameToTextureLUT;
    std::unordered_map<std::string, Handle<Material>, StringHash, std::equal_to<>> nameToMaterialLUT;

    Pool<Buffer> bufferPool{10};
    Pool<Mesh> meshPool{10};
    Pool<Texture> texturePool{10};
    Pool<Material> materialPool{10};
};