#include "Scene.hpp"

Scene::Scene()
{
    // 0th transform is used as root
    transforms.emplace_back();
}

MeshObject& Scene::createMeshObject()
{
    auto& meshObject = meshObjects.emplace_back();
    uint32_t transformIndex = transforms.size();
    transforms.emplace_back();
}
