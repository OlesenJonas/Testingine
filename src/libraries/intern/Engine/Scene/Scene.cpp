#include "Scene.hpp"

#include <ECS/ECS.hpp>
#include <Engine/Engine.hpp>

SceneObject::SceneObject(ECS::Entity entity) : entity(entity){};
Renderable::Renderable(ECS::Entity entity) : SceneObject(entity){};

Scene::Scene(ECS& ecs) : ecs(ecs), root(ecs.createEntity())
{
    ecs.registerComponent<Transform>();
    ecs.registerComponent<Hierarchy>();
    ecs.registerComponent<RenderInfo>();

    Transform& rootTransform = *root.addComponent<Transform>();
    rootTransform.calculateLocalTransformMatrix();
    root.addComponent<Hierarchy>();
}

Transform* SceneObject::getTransform()
{
    return entity.getComponent<Transform>();
}

Renderable Scene::addRenderable()
{
    Renderable renderable{ecs.createEntity()};
    renderable.entity.addComponent<Transform>();
    renderable.entity.addComponent<Hierarchy>();
    renderable.entity.addComponent<RenderInfo>();

    return renderable;
}

RenderInfo* Renderable::getRenderInfo()
{
    return entity.getComponent<RenderInfo>();
}