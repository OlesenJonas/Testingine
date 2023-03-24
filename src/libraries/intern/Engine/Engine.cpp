#include "Engine.hpp"

Engine::Engine() : mainWindow(1200, 800, "Vulkan Test", {{GLFW_MAXIMIZED, GLFW_TRUE}})
{
    ptr = this;

    renderer.init();

    mainCamera =
        Camera{static_cast<float>(mainWindow.width) / static_cast<float>(mainWindow.height), 0.1f, 1000.0f};

    inputManager.init();
}

Engine::~Engine()
{
    renderer.cleanup();
}

void Engine::run()
{
    renderer.run();
}