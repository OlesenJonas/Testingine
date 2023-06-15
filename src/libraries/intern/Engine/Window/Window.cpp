#include "Window.hpp"

#include <cassert>

Window::Window(int width, int height, const char* title, std::initializer_list<WindowHint> hints)
    : width(width), height(height)
{
    assert(width > 0 && height > 0);

    if(!glfwInitialized)
    {
        // not thread safe of course, but how often do 2 threads create windows at the same time
        //  (not that thats an excuse)
        glfwInit();
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    for(const WindowHint& hint : hints)
    {
        glfwWindowHint(hint.first, hint.second);
    }

    glfwWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);

    // retrieve the actual window size in case any flags caused the window to be created with a size thats
    // different from the passed parameters
    glfwGetWindowSize(glfwWindow, &this->width, &this->height);

    assert(glfwWindow != nullptr);
}