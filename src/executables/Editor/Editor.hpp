#pragma once

#include <ECS/ECS.hpp>
#include <Engine/Application/Application.hpp>

class Editor final : public Application
{
  public:
    Editor();
    ~Editor() final;

    void update();

  private:
    // TODO: REMOVE AFTER VULKAN RENDERER REFACTOR
    struct UserData
    {
        ECS* ecs;
        Camera* cam;
    } userData;

    InputManager inputManager;

    ECS ecs;
    ECS::Entity sceneRoot;

    // TODO: not sure if camere should be part of just Editor, or Application is general
    Camera mainCamera;
};