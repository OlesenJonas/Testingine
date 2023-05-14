#pragma once

#include "DefaultComponents.hpp"

#include <ECS/ECS.hpp>

/*
    TODO:
        (re)think what ownership of a Renderable means, and clarify that
            (Currently it means nothing, even the Entity member is just a handle and doesnt actually own anything)
        not sure how to make sure that stuff gets properly deleted, and when thats even requried
        also what about leaking entities?
            I guess the scene should store all entities somewhere, so if a renderable object gets deleted,
            its still possible to retrieve the underlying entity later on
            (imagine function creates and alters renderable, then exists. Renderable should still be part of scene
            and may need to be changed later)

        Not sure if I even want the SceneObject/Renderable/etc inheritence...
            Perhaps usign the raw entitys isnt that bad, would just need to find a way to combine that with scene
            hierarchy somehow
*/

class Renderable;
class Scene;

class SceneObject
{
  public:
    Transform* getTransform();

  private:
    explicit SceneObject(ECS::Entity entity);
    ECS::Entity entity;

    friend Renderable;
    friend Scene;
};

struct Renderable : public SceneObject
{
  public:
    RenderInfo* getRenderInfo();

  private:
    explicit Renderable(ECS::Entity entity);

    friend Scene;
};

class Scene
{
  public:
    Scene(ECS& ecs);

    Renderable addRenderable();

  private:
    // root is handled as an entity without any additional components and the identity transform
    ECS::Entity root;
    ECS& ecs;
};