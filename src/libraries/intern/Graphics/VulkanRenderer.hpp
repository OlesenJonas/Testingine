#pragma once

#include "VulkanTypes.hpp"

struct GLFWwindow;

class VulkanRenderer
{
  public:
    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();

    // run main loop
    void run();

    bool isInitialized{false};

    int frameNumber{0};
    GLFWwindow* window = nullptr;
    VkExtent2D windowExtent{1700, 900};

#ifdef NDEBUG
    bool enableValidationLayers = false;
#else
    bool enableValidationLayers = true;
#endif
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkSurfaceKHR surface;

  private:
    void initVulkan();
};