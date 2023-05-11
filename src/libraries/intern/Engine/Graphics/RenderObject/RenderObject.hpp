#pragma once

#include <glm/glm.hpp>

#include <Datastructures/Pool.hpp>

struct Mesh;
struct MaterialInstance;

struct RenderObject
{
    Handle<Mesh> mesh;

    Handle<MaterialInstance> materialInstance;

    glm::mat4 transformMatrix;
};