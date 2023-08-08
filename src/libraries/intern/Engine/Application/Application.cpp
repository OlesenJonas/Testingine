#include "Application.hpp"

Application::Application(CreateInfo&& info)
    : mainWindow(info.windowWidth, info.windowHeight, info.name.c_str(), info.windowHints)
{
    // ensure global services are initialized in correct order

    renderer.init();
    // want display output
    renderer.setupSwapchain(mainWindow.glfwWindow);

    resourceManager.init();
}

Application::~Application()
{
    resourceManager.cleanup();
    renderer.cleanup();
}