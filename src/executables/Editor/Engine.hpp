#pragma once

#include <ECS/ECS.hpp>
#include <Engine/Application/Application.hpp>

class Engine final : public Application
{
  public:
    Engine();
    ~Engine() final;

    void update();

  private:
    InputManager inputManager;

    ECS ecs;
    ECS::Entity sceneRoot;

    Camera mainCamera;
};