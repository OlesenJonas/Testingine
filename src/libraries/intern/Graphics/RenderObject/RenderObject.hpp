#pragma once

#include <glm/glm.hpp>

#include <intern/Datastructures/Pool.hpp>

#include "../Material/Material.hpp"
#include "../Mesh/Mesh.hpp"

struct RenderObject
{
    Handle<Mesh> mesh;

    Handle<Material> material;

    glm::mat4 transformMatrix;
};