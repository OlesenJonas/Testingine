#include "Scene.hpp"
#include "glTF/glTF.hpp"
#include "glTF/glTFtoVK.hpp"

#include <Datastructures/Pool.hpp>
#include <Engine.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <filesystem>
#include <fstream>
#include <vulkan/vulkan_core.h>

void Scene::load(std::string path)
{
    std::filesystem::path basePath{path};
    basePath = basePath.parent_path();

    const glTF::Main json = glTF::Main::load(path);
    assert(json.asset.version == "2.0");

    ResourceManager* rm = Engine::get()->getResourceManager();

    // Load samplers
    std::vector<Handle<Sampler>> samplers;
    samplers.resize(json.samplers.size());
    for(int i = 0; i < json.samplers.size(); i++)
    {
        auto& samplerInfo = json.samplers[i];
        samplers[i] = rm->createSampler(Sampler::Info{
            .magFilter = glTF::toVk::magFilter(samplerInfo.magFilter),
            .minFilter = glTF::toVk::minFilter(samplerInfo.minFilter),
            .mipmapMode = glTF::toVk::mipmapMode(samplerInfo.minFilter),
            .addressModeU = glTF::toVk::addressMode(samplerInfo.wrapS),
            .addressModeV = glTF::toVk::addressMode(samplerInfo.wrapT),
        });
    }

    // Load textures
    std::vector<Handle<Texture>> textures;
    textures.resize(json.images.size());
    for(int i = 0; i < json.images.size(); i++)
    {
        std::filesystem::path imagePath = basePath / json.images[i].uri;
        textures[i] = rm->createTexture(imagePath.generic_string().c_str(), VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    // Load buffers
    std::vector<std::vector<char>> buffers;
    buffers.resize(json.buffers.size());
    for(int i = 0; i < json.buffers.size(); i++)
    {
        const auto& bufferInfo = json.buffers[i];
        std::filesystem::path bufferPath = basePath / bufferInfo.uri;

        std::ifstream file(bufferPath.c_str(), std::ios::ate | std::ios::binary);
        if(!file.is_open())
        {
            assert(false); // TODO: error handling
        }
        std::vector<char>& buffer = buffers[i];
        auto fileSize = (size_t)file.tellg();
        assert(fileSize == bufferInfo.byteLength);
        buffer.resize(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        file.seekg(0);
    }
}