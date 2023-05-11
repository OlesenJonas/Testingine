#pragma once

#include <ECS/ECS.hpp>
#include <Engine/Camera/Camera.hpp>
#include <Engine/Graphics/Renderer/VulkanRenderer.hpp>
#include <Engine/InputManager/InputManager.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <Engine/Scene/Scene.hpp>
#include <Engine/Window/Window.hpp>

class Engine
{
  public:
    Engine();
    ~Engine();

    [[nodiscard]] static inline Engine* get()
    {
        return ptr;
    }

    bool isRunning() const;

    void update();

    inline Camera* getCamera()
    {
        return &mainCamera;
    }
    inline InputManager* getInputManager()
    {
        return &inputManager;
    }
    inline Window* getMainWindow()
    {
        return &mainWindow;
    }
    inline VulkanRenderer* getRenderer()
    {
        return &renderer;
    }
    inline ResourceManager* getResourceManager()
    {
        return &resourceManager;
    }

    ECS ecs;
    Scene scene;

  private:
    bool _isRunning = true;

    inline static Engine* ptr = nullptr;
    InputManager inputManager;
    Window mainWindow;
    VulkanRenderer renderer;
    ResourceManager resourceManager;
    Camera mainCamera;
};