#include "Editor.hpp"
#include <ImGui/imgui.h>

void Editor::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
    Camera& activeCam = editor->inDebugView ? editor->debugCam : editor->mainCam;

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
        activeCam.setMode(Camera::Mode::FLY);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        activeCam.setMode(Camera::Mode::ORBIT);
    }
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // ...
    }
}

void Editor::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

// TODO: should be part of editor !!
void Editor::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
    Camera& activeCam = editor->inDebugView ? editor->debugCam : editor->mainCam;
    // IsWindowHovered enough? or ImGui::getIO().WantCapture[Mouse/Key]
    if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        if(activeCam.getMode() == Camera::Mode::ORBIT)
        {
            activeCam.changeRadius(yoffset < 0);
        }
        else if(activeCam.getMode() == Camera::Mode::FLY)
        {
            float factor = (yoffset > 0) ? 1.1f : 1 / 1.1f;
            activeCam.setFlySpeed(activeCam.getFlySpeed() * factor);
        }
    }
}

void Editor::resizeCallback(GLFWwindow* window, int width, int height)
{
    // todo
}