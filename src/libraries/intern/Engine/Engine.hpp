#pragma once

#include <intern/Camera/Camera.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>
#include <intern/InputManager/InputManager.hpp>
#include <intern/Window/Window.hpp>

class Engine
{
  public:
    inline static Engine* ptr = nullptr;
    Engine();
    ~Engine();

    void run();

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

  private:
    InputManager inputManager;
    Camera mainCamera;
    Window mainWindow;
    VulkanRenderer renderer;
};