#pragma once

#include <intern/Camera/Camera.hpp>
#include <intern/ECS/ECS.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>
#include <intern/InputManager/InputManager.hpp>
#include <intern/ResourceManager/ResourceManager.hpp>
#include <intern/Scene/Scene.hpp>
#include <intern/Window/Window.hpp>

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