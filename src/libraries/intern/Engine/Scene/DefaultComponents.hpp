#pragma once

#include <Datastructures/Pool.hpp>
#include <ECS/ECS.hpp>
#include <glm/glm.hpp>
#include <vector>

struct MaterialInstance;
struct Mesh;

struct Transform
{
    glm::vec3 position;
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::vec3 angle; // switch to quaternions

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
    Handle<Mesh> mesh;
    Handle<MaterialInstance> materialInstance;
};