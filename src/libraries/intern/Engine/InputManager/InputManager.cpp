#include "InputManager.hpp"

#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <limits>

void InputManager::init(GLFWwindow* window)
{
    glfwGetCursorPos(window, &mouseX, &mouseY);
    oldMouseX = mouseX; // NOLINT
    oldMouseY = mouseY; // NOLINT
}

void InputManager::resetTime(int64_t frameCount, double simulationTime)
{
    this->frameCount = frameCount;
    this->simulationTime = simulationTime;
}

void InputManager::disableFixedTimestep()
{
    useFixedTimestep = false;
}

void InputManager::enableFixedTimestep(double timestep)
{
    useFixedTimestep = true;
    fixedDeltaTime = timestep;
}

void InputManager::update(GLFWwindow* window)
{
    frameCount++;
    double currentRealTime = glfwGetTime();
    deltaTime = currentRealTime - realTime;
    realTime = currentRealTime;

    simulationTime += useFixedTimestep ? fixedDeltaTime : deltaTime;

    glfwGetCursorPos(window, &mouseX, &mouseY);
    mouseDelta = {mouseX - oldMouseX, mouseY - oldMouseY};
    oldMouseX = mouseX;
    oldMouseY = mouseY;
}

void InputManager::setupCallbacks(
    GLFWwindow* window,
    GLFWkeyfun keyCallback,
    GLFWmousebuttonfun mousebuttonCallback,
    GLFWscrollfun scrollCallback,
    GLFWframebuffersizefun resizeCallback)
{

    if(ImGui::GetCurrentContext() != nullptr)
    {
        // Need to re-set ImGui callbacks if ImGui was already initialized
        ImGui_ImplGlfw_RestoreCallbacks(window);
    }

    if(keyCallback != nullptr)
        glfwSetKeyCallback(window, keyCallback);

    if(mousebuttonCallback != nullptr)
        glfwSetMouseButtonCallback(window, mousebuttonCallback);

    if(scrollCallback != nullptr)
        glfwSetScrollCallback(window, scrollCallback);

    if(resizeCallback != nullptr)
        glfwSetFramebufferSizeCallback(window, resizeCallback);

    if(ImGui::GetCurrentContext() != nullptr)
    {
        // Need to re-set ImGui callbacks if ImGui was already initialized
        ImGui_ImplGlfw_InstallCallbacks(window);
    }
}