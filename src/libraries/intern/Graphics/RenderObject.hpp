#pragma once

#include <glm/glm.hpp>

#include "Material.hpp"
#include "Mesh.hpp"

struct RenderObject
{
    Mesh* mesh;

    Material* material;

    glm::mat4 transformMatrix;
};