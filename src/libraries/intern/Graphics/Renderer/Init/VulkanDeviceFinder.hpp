#pragma once

#include "../../VulkanTypes.hpp"
#include "Graphics/VulkanTypes.hpp"
#include <vulkan/vulkan.h>

#include "Datastructures/Span.hpp"

#include <vector>

struct VulkanDeviceFinder
{
    VulkanDeviceFinder(VkInstance instance);

    void setSurface(VkSurfaceKHR surface);
    void setValidationLayers(Span<const char* const> validationLayers);
    void setExtensions(Span<const char* const> deviceExtensions);

    VkPhysicalDevice findPhysicalDevice();
    VkDevice createLogicalDevice();
    QueueFamilyIndices getQueueFamilyIndices();

  private:
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    VkInstance instance;
    VkSurfaceKHR surface;
    bool enableValidationLayers = false;
    std::vector<const char*> validationLayers;
    std::vector<const char*> deviceExtensions;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    QueueFamilyIndices queueFamilyIndices;
};