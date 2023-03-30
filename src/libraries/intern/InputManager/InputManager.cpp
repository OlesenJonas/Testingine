#include "InputManager.hpp"
#include "../Engine/Engine.hpp"

#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <limits>

void InputManager::init()
{
    glfwGetCursorPos(Engine::get()->getMainWindow()->glfwWindow, &mouseX, &mouseY);
    oldMouseX = mouseX; // NOLINT
    oldMouseY = mouseY; // NOLINT
    setupCallbacks();
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

void InputManager::update()
{
    frameCount++;
    double currentRealTime = glfwGetTime();
    deltaTime = currentRealTime - realTime;
    realTime = currentRealTime;

    simulationTime += useFixedTimestep ? fixedDeltaTime : deltaTime;

    glfwGetCursorPos(Engine::get()->getMainWindow()->glfwWindow, &mouseX, &mouseY);
    mouseDelta = {mouseX - oldMouseX, mouseY - oldMouseY};
    oldMouseX = mouseX;
    oldMouseY = mouseY;
}

void InputManager::setupCallbacks(
    GLFWkeyfun keyCallback,
    GLFWmousebuttonfun mousebuttonCallback,
    GLFWscrollfun scrollCallback,
    GLFWframebuffersizefun resizeCallback)
{
    GLFWwindow* window = Engine::get()->getMainWindow()->glfwWindow;
    if(keyCallback != nullptr)
        glfwSetKeyCallback(window, keyCallback);
    else
        glfwSetKeyCallback(window, defaultKeyCallback);

    if(mousebuttonCallback != nullptr)
        glfwSetMouseButtonCallback(window, mousebuttonCallback);
    else
        glfwSetMouseButtonCallback(window, defaultMouseButtonCallback);

    if(scrollCallback != nullptr)
        glfwSetScrollCallback(window, scrollCallback);
    else
        glfwSetScrollCallback(window, defaultScrollCallback);

    if(resizeCallback != nullptr)
        glfwSetFramebufferSizeCallback(window, resizeCallback);
    else
        glfwSetFramebufferSizeCallback(window, defaultResizeCallback);

    glfwSetWindowUserPointer(window, reinterpret_cast<void*>(Engine::get()));
}

//// STATIC FUNCTIONS ////

void InputManager::defaultMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    // IsWindowHovered enough? or ImGui::getIO().WantCapture[Mouse/Key]
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByPopup))
    {
        return;
    }

    // if((button == GLFW_MOUSE_BUTTON_MIDDLE || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_PRESS)
    // {
    //     glfwGetCursorPos(window, &cbStruct->mousePos->x, &cbStruct->mousePos->y);
    // }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        Engine::get()->getCamera()->setMode(Camera::Mode::FLY);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        Engine::get()->getCamera()->setMode(Camera::Mode::ORBIT);
    }
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // ...
    }
}

void InputManager::defaultKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Context& ctx = *static_cast<Context*>(glfwGetWindowUserPointer(window));
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

void InputManager::defaultScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    Context& ctx = *static_cast<Context*>(glfwGetWindowUserPointer(window));
    // IsWindowHovered enough? or ImGui::getIO().WantCapture[Mouse/Key]
    if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        // Camera* cam = ctx.getCamera();
        Camera* cam = Engine::get()->getCamera();
        if(cam->getMode() == Camera::Mode::ORBIT)
        {
            cam->changeRadius(yoffset < 0);
        }
        else if(cam->getMode() == Camera::Mode::FLY)
        {
            float factor = (yoffset > 0) ? 1.1f : 1 / 1.1f;
            cam->setFlySpeed(cam->getFlySpeed() * factor);
        }
    }
}

void InputManager::defaultResizeCallback(GLFWwindow* window, int width, int height)
{
    // todo
}