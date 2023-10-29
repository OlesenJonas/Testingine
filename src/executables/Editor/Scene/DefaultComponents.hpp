#pragma once

#include <Datastructures/Pool/Pool.hpp>
#include <ECS/ECS.hpp>
#include <Engine/Graphics/Material/Material.hpp>
#include <Engine/Graphics/Mesh/Mesh.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

struct Transform
{
    glm::vec3 position{0.f, 0.f, 0.f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::quat orientation;

    glm::mat4 localTransform{1.0f};

    glm::mat4 localToWorld{1.0f};

    void calculateLocalTransformMatrix();
};

struct Hierarchy
{
    ECS::Entity parent;
    std::vector<ECS::Entity> children;
};

// TODO: where to put this
template <typename T, size_t N>
consteval auto FilledArray(T value)
{
    std::array<T, N> arr;
    std::fill(arr.begin(), arr.end(), value);
    return arr;
}

struct MeshRenderer
{
    std::array<Mesh::Handle, Mesh::MAX_SUBMESHES> subMeshes =
        FilledArray<Mesh::Handle, Mesh::MAX_SUBMESHES>(Mesh::Handle::Invalid());

    // TODO: also store Material::Handle-s here for less indirections?
    std::array<MaterialInstance::Handle, Mesh::MAX_SUBMESHES> materialInstances =
        FilledArray<MaterialInstance::Handle, Mesh::MAX_SUBMESHES>(MaterialInstance::Handle::Invalid());

    // GPU Render item info
    std::array<uint32_t, Mesh::MAX_SUBMESHES> instanceBufferIndices =
        FilledArray<uint32_t, Mesh::MAX_SUBMESHES>(0xFFFFFFFF);
};