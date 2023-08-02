#pragma once

#include <Engine/Camera/Camera.hpp>
#include <Engine/Graphics/Renderer/VulkanRenderer.hpp>
#include <Engine/InputManager/InputManager.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <Engine/Window/Window.hpp>

struct ApplicationCreateInfo
{
    std::string name = "Default Application";
    int windowWidth = 1280;
    int windowHeight = 720;
    std::initializer_list<Window::WindowHint> windowHints = {};
};

class Application
{
  public:
    // TODO: REMOVE !! ONCE VULKAN RENDERER PROPERLY REFACTORED !
    inline static Application* ptr = nullptr;

    explicit Application(ApplicationCreateInfo&& info);
    virtual ~Application();

    // TODO: REMOVE
    void* userData = nullptr;

    // TODO: MAKE PROTECTED AGAIN ONCE VULKAN RENDERER LOGIC SEPERATED OUT !
  public:
    //   protected:
    bool _isRunning = true;

    Window mainWindow;
    VulkanRenderer renderer;
    ResourceManager resourceManager;
};