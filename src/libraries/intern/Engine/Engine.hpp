#pragma once

#include <intern/Camera/Camera.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>
#include <intern/InputManager/InputManager.hpp>

struct GLFWwindow;

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
    inline GLFWwindow** getMainWindow()
    {
        return &mainWindow;
    }

  private:
    InputManager inputManager;
    Camera mainCamera;
    // todo: abstract into window class that stores width, height, amount of images, etc aswell
    GLFWwindow* mainWindow = nullptr;
    VulkanRenderer renderer;
};