#pragma once

#include <ECS/ECS.hpp>
#include <glm/glm.hpp>
#include <string>

class ResourceManager;

struct Scene
{
    static void load(std::string path, ECS* ecs, ECS::Entity parent);

    static void updateTransformHierarchy(ECS::Entity entity, glm::mat4 parentToWorld);
};