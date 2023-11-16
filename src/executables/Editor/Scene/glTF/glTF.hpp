#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
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
    struct TextureTransform;
    struct TextureExtensions;
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
    glm::vec3 translation;
    glm::vec4 rotationAsVec;
    glm::vec3 scale;
};

struct glTF::PrimitiveAttributes
{
    int normalAccessor;
    int positionAccessor;
    int uv0Accessor;
    std::optional<int> uv1Accessor;
    std::optional<int> uv2Accessor;
    // std::optional<int> tangentAccessor;
};

struct glTF::Primitive
{
    PrimitiveAttributes attributes;
    std::optional<int> indexAccessor;
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

    enum ComponentType
    {
        sint8 = 5120,
        uint8 = 5121,
        sint16 = 5122,
        uint16 = 5123,
        uint32 = 5125,
        f32 = 5126,
    };

    static constexpr std::string_view scalar = "SCALAR";
    static constexpr std::string_view vec2 = "VEC2";
    static constexpr std::string_view vec3 = "VEC3";
    static constexpr std::string_view vec4 = "VEC4";
    // todo: matrix types

    static constexpr std::array<uint32_t, 7> componentTypeByteSizes{
        sizeof(int8_t),
        sizeof(uint8_t),
        sizeof(int16_t),
        sizeof(uint16_t),
        sizeof(int32_t),
        sizeof(uint32_t),
        sizeof(float)};

    static inline constexpr uint32_t getComponentTypeSize(ComponentType type)
    {
        return componentTypeByteSizes[type - ComponentType::sint8];
    }
};

struct glTF::TextureTransform
{
    glm::vec2 offset;
    glm::vec2 scale;
};

struct glTF::TextureExtensions
{
    std::optional<TextureTransform> transform;
};

struct glTF::TextureParams
{
    // todo: additional parameters, like UV-set, UV-scale, tint etc, etc...
    uint32_t index;
    uint32_t uvSet;
    std::optional<TextureExtensions> extensions;
};

struct glTF::PBRMetalRoughParams
{
    TextureParams baseColorTexture;
    std::optional<TextureParams> metallicRoughnessTexture;
    float metallicFactor;
    float roughnessFactor;
};

struct glTF::Material
{
    PBRMetalRoughParams pbrMetallicRoughness;
    std::optional<TextureParams> occlusionTexture;
    std::optional<TextureParams> normalTexture;
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

    static Main load(std::string path);
};