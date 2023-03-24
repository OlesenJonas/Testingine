#pragma once

#include <GLFW/glfw3.h>
#include <initializer_list>
#include <utility>

class Window
{
  public:
    using WindowHint = std::pair<int, int>;
    Window(int width, int height, const char* title, std::initializer_list<WindowHint> hints = {});
    GLFWwindow* glfwWindow = nullptr;

    int width = 0;
    int height = 0;

  private:
    inline static bool glfwInitialized = false;
};