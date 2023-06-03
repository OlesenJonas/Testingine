#pragma once

#include <ECS/ECS.hpp>
#include <glm/glm.hpp>
#include <string>

struct Scene
{
    static void load(std::string path);

    static void updateTransformHierarchy(ECS::Entity entity, glm::mat4 parentToWorld);
};