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
    glm::vec3 position;
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::quat orientation;

    glm::mat4 localTransform{1.0f};
    glm::mat4 localToWorld{1.0f};

    void calculateLocalTransformMatrix();
};

struct Hierarchy
{
    ECS::EntityID parent = 0;
    std::vector<ECS::EntityID> children;
};

struct RenderInfo
{
    Mesh::Handle mesh;
    MaterialInstance::Handle materialInstance;
};