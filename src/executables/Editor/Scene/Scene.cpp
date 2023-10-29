#include "Scene.hpp"

#include "DefaultComponents.hpp"

Scene::Scene() : root(ECS::impl()->createEntity())
{
    auto& ecs = *ECS::impl();
    ecs.registerComponent<Transform>();
    ecs.registerComponent<Hierarchy>();
    ecs.registerComponent<MeshRenderer>();

    root.addComponent<Transform>();
    root.addComponent<Hierarchy>(root);
}

ECS::Entity Scene::createEntity() { return createEntity(root); }

ECS::Entity Scene::createEntity(ECS::Entity parent)
{
    ECS::Entity newEntt = ECS::impl()->createEntity();
    newEntt.addComponent<Transform>();
    newEntt.addComponent<Hierarchy>(parent);

    return newEntt;
}