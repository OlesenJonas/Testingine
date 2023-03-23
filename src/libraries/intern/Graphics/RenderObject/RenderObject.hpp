#pragma once

#include <glm/glm.hpp>

#include "../Material/Material.hpp"
#include "../Mesh/Mesh.hpp"

struct RenderObject
{
    Mesh* mesh;

    Material* material;

    glm::mat4 transformMatrix;
};