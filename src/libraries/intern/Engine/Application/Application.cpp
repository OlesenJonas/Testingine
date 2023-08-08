#include "Application.hpp"

Application::Application(CreateInfo&& info)
    : mainWindow(info.windowWidth, info.windowHeight, info.name.c_str(), info.windowHints)
{
    // ensure global services are initialized in correct order

    renderer.init(mainWindow.glfwWindow);

    resourceManager.init();
}

Application::~Application()
{
    resourceManager.cleanup();
    renderer.cleanup();
}