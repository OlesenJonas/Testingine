#pragma once

#include "../HelperTypes.hpp"

#include <Datastructures/Span.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    bool enableValidationLayers = false;
    Span<const char* const> validationLayers;
    Span<const char* const> deviceExtensions;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices;
};