#include "Engine.hpp"
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>

Engine::Engine() : mainWindow(1200, 800, "Vulkan Test", {{GLFW_MAXIMIZED, GLFW_TRUE}})
{
    ptr = this;

    resourceManager.init();
    renderer.init();

    mainCamera =
        Camera{static_cast<float>(mainWindow.width) / static_cast<float>(mainWindow.height), 0.1f, 1000.0f};

    inputManager.init();

    glfwSetTime(0.0);
    Engine::get()->getInputManager()->resetTime();
}

Engine::~Engine()
{
    renderer.waitForWorkFinished();
    resourceManager.cleanup();
    renderer.cleanup();
}

bool Engine::isRunning() const
{
    return _isRunning;
}

void Engine::update()
{
    _isRunning &= !glfwWindowShouldClose(Engine::get()->getMainWindow()->glfwWindow);
    if(!_isRunning)
        return;

    glfwPollEvents();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    inputManager.update();
    if(!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
    {
        mainCamera.update();
    }
    // Not sure about the order of UI & engine code

    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::Render();

    renderer.draw();
}