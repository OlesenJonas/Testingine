#pragma once

#include "Application.hpp"

Application::Application(ApplicationCreateInfo&& info)
    : mainWindow(info.windowWidth, info.windowHeight, info.name.c_str(), info.windowHints)
{
    resourceManager.init();
    renderer.init(mainWindow.glfwWindow);
}

Application::~Application()
{
    resourceManager.cleanup();
    renderer.cleanup();
}