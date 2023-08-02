#include "InputManager.hpp"

#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <limits>

// TODO: REMOVE
#include <ECS/ECS.hpp>
#include <Engine/Application/Application.hpp>
#include <Engine/Camera/Camera.hpp>

void InputManager::init(GLFWwindow* window)
{
    glfwGetCursorPos(window, &mouseX, &mouseY);
    oldMouseX = mouseX; // NOLINT
    oldMouseY = mouseY; // NOLINT
    setupCallbacks(window);
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
}

//// STATIC FUNCTIONS ////

void InputManager::defaultMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    struct UserData
    {
        ECS* ecs;
        Camera* cam;
    };
    auto* userData = (UserData*)Application::ptr->userData;
    Camera* cam = userData->cam;
    assert(userData != nullptr);
    assert(cam != nullptr);

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
        cam->setMode(Camera::Mode::FLY);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        cam->setMode(Camera::Mode::ORBIT);
    }
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // ...
    }
}

void InputManager::defaultKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

// TODO: should be part of editor !!
void InputManager::defaultScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // IsWindowHovered enough? or ImGui::getIO().WantCapture[Mouse/Key]
    if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {

        struct UserData
        {
            ECS* ecs;
            Camera* cam;
        };
        auto* userData = (UserData*)Application::ptr->userData;
        Camera* cam = userData->cam;
        assert(userData != nullptr);
        assert(cam != nullptr);

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