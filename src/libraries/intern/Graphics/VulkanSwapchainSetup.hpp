#pragma once

#include "VulkanTypes.hpp"
#include <vulkan/vulkan.h>

#include <vector>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;

struct VulkanSwapchainSetup
{
    VulkanSwapchainSetup(
        VkPhysicalDevice physicalDevice, VkDevice device, GLFWwindow* window, VkSurfaceKHR surface);

    void setup(uint32_t graphicsQueueFamilyIndex, uint32_t presentQueueFamilyIndex);

    // clang-format off
    inline VkSwapchainKHR getSwapchain() const {return swapchain;};
    inline VkExtent2D getSwapchainExtent() const {return swapchainExtent;}
    inline VkFormat getSwapchainImageFormat() const {return swapchainImageFormat;}
    inline std::vector<VkImage> getSwapchainImages() const {return swapchainImages;}
    // clang-format on
    std::vector<VkImageView> createSwapchainImageViews();

  private:
    SwapchainSupportDetails querySwapchainSupport();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Input
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    GLFWwindow* window;
    VkSurfaceKHR surface;

    // Output
    VkSwapchainKHR swapchain;
    VkExtent2D swapchainExtent;
    VkFormat swapchainImageFormat;
    std::vector<VkImage> swapchainImages;
};