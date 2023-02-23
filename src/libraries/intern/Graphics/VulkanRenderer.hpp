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

    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    QueueFamilyIndices queueFamilyIndices;

    VkSwapchainKHR swapchain;
    // On high-dpi monitors, swapChainExtent is not necessarily window extent!
    VkExtent2D swapchainExtent{1700, 900};
    VkFormat swapchainImageFormat;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

  private:
    void initVulkan();
    void initSwapchain();
};