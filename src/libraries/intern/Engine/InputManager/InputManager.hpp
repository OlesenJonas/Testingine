#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>

// TODO: not sure I like "the same" window being passed to all these functions

class InputManager
{
  public:
    InputManager() = default;

    void init(GLFWwindow* window);

    void update(GLFWwindow* window);

    void resetTime(int64_t frameCount = 0, double simulationTime = 0.0);
    void disableFixedTimestep();
    void enableFixedTimestep(double timestep);
    void setupCallbacks(
        GLFWwindow* window,
        GLFWkeyfun keyCallback = nullptr,
        GLFWmousebuttonfun mousebuttonCallback = nullptr,
        GLFWscrollfun scrollCallback = nullptr,
        GLFWframebuffersizefun resizeCallback = nullptr);

    inline glm::vec2 getMouseDelta() const
    {
        return mouseDelta;
    }

    inline glm::vec2 getMousePos() const
    {
        return {mouseX, mouseY};
    }

    inline float getRealDeltaTime() const
    {
        return static_cast<float>(deltaTime);
    }
    inline float getSimulationDeltaTime() const
    {
        return static_cast<float>(useFixedTimestep ? fixedDeltaTime : deltaTime);
    }
    inline double getSimulationTime() const
    {
        return simulationTime;
    };
    inline int64_t getFrameCount() const
    {
        return frameCount;
    };

  private:
    /* Track simulation and real time seperately so that input etc still function correctly */
    int64_t frameCount = -1;
    double realTime = 0.0;
    double simulationTime = 0.0;
    double fixedDeltaTime = 0.0;
    double deltaTime = 0.0;
    bool useFixedTimestep = false;

    double mouseX;
    double mouseY;
    double oldMouseX;
    double oldMouseY;
    glm::vec2 mouseDelta{0.0f, 0.0f};
};
