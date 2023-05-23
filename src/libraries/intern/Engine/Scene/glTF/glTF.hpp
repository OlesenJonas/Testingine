#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace glTF
{
    struct AssetInfo;
    struct Scene;
    struct Node;
    struct Main;
    struct Mesh;
    struct PrimitiveAttributes;
    struct Primitive;
    struct Accessor;
    struct TextureParams;
    struct PBRMetalRoughParams;
    struct Material;
    struct Texture;
    struct Image;
    struct Sampler;
    struct BufferView;
    struct Buffer;
} // namespace glTF

struct glTF::AssetInfo
{
    // can ignore the first two, not relevent atm
    // std::string copyright;
    // std::string generator;
    std::string version;
};

struct glTF::Scene
{
    std::vector<int> nodeIndices;
};

struct glTF::Node
{
    std::optional<int> meshIndex = 0xFFFFFFFF;
    std::optional<std::vector<int>> childNodeIndices;
};

struct glTF::PrimitiveAttributes
{
    int normalAccessor;
    int positionAccessor;
    int uv0Accessor;
    // tangent accessor?
};

struct glTF::Primitive
{
    PrimitiveAttributes attributes;
    int indexAccessor;
    int materialIndex;
};

struct glTF::Mesh
{
    std::vector<Primitive> primitives;
    std::string name;
};

struct glTF::Accessor
{
    uint32_t bufferViewIndex;
    uint32_t byteOffset;
    uint32_t componentType;
    uint32_t count;
    std::string type;
};

struct glTF::TextureParams
{
    // todo: additional parameters, like UV-set, UV-scale, tint etc, etc...
    uint32_t index;
};

struct glTF::PBRMetalRoughParams
{
    TextureParams baseColorTexture;
    TextureParams metallicRoughnessTexture;
};

struct glTF::Material
{
    PBRMetalRoughParams metallicRoughness;
    TextureParams occlusionTexture;
    TextureParams normalTexture;
    // todo: parse other members
};
struct glTF::Texture
{
    uint32_t samplerIndex;
    uint32_t sourceIndex;
};

struct glTF::Image
{
    std::string uri;
};

struct glTF::Sampler
{
    uint32_t magFilter;
    uint32_t minFilter;
    uint32_t wrapS;
    uint32_t wrapT;
};

struct glTF::BufferView
{
    uint32_t bufferIndex;
    uint32_t byteOffset;
    uint32_t byteLength;
    uint32_t byteStride;
};

struct glTF::Buffer
{
    uint32_t byteLength;
    std::string uri;
};

struct glTF::Main
{
    AssetInfo asset;
    int defaultScene = 0;
    std::vector<Scene> scenes;
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Accessor> accessors;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<Image> images;
    std::vector<Sampler> samplers;
    std::vector<BufferView> bufferViews;
    std::vector<Buffer> buffers;

    static const Main load(std::string path);
};