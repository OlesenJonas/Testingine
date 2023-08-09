#include "Application.hpp"

Application::Application(CreateInfo&& info)
    : mainWindow(info.windowWidth, info.windowHeight, info.name.c_str(), info.windowHints)
{
    // ensure global services are initialized in correct order

    gfxDevice.init(mainWindow.glfwWindow);

    resourceManager.init();
}

Application::~Application()
{
    resourceManager.cleanup();
    gfxDevice.cleanup();
}