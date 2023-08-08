#pragma once

#include <Datastructures/Span.hpp>
#include <Engine/Camera/Camera.hpp>
#include <Engine/Graphics/Renderer/VulkanRenderer.hpp>
#include <Engine/InputManager/InputManager.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <Engine/Window/Window.hpp>

class Application
{
  public:
    struct CreateInfo
    {
        std::string name = "Default Application";
        int windowWidth = 1280;
        int windowHeight = 720;
        Span<const Window::WindowHint> windowHints = {};
    };
    explicit Application(CreateInfo&& info);
    virtual ~Application();

  protected:
    bool _isRunning = true;

    Window mainWindow;
    VulkanRenderer renderer;
    ResourceManager resourceManager;
};