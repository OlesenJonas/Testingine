#pragma once

#include <Datastructures/Pool.hpp>
#include <glm/glm.hpp>
#include <vector>

struct Mesh;
struct MaterialInstance;

// inspired by https://www.david-colson.com/2020/02/09/making-a-simple-ecs.html

struct MeshObject
{
    Handle<Mesh> mesh;
    Handle<MaterialInstance> material;

    uint32_t transformIndex = 0xFFFFFFFF;
};

class Scene
{
  public:
    // TODO: real ECS, but for now only transform and transform hierarchy are needed
    struct TransformComponent
    {
        glm::vec3 position;
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 angle; // switch to quaternions

        glm::mat4 localTransform{1.0f};

        uint32_t parent = 0;
        std::vector<uint32_t> children;
    };

    Scene();
    // todo: dont return a reference, that will break on vector growth
    //       but works for now, and when adding a "real" ECS this will be changed
    MeshObject& createMeshObject();

  private:
    // todo: ComponentPool type? (would it differ from the other Pool structure?)
    std::vector<TransformComponent> transforms;
    std::vector<MeshObject> meshObjects;
};