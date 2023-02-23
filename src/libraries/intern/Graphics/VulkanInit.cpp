#include "VulkanInit.hpp"
#include "VulkanDebug.hpp"

#include <initializer_list>
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>

VkInstance createInstance(bool enableValidationLayers, Span<const char* const> validationLayers)
{
    if(enableValidationLayers && !checkValidationLayerSupport(validationLayers))
    {
        throw std::runtime_error("validation layers requested, but not available");
    }

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3};

    auto extensions = getRequiredExtensions(enableValidationLayers);
    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        // Extensions
        .enabledExtensionCount = (uint32_t)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };

    auto debugCreateInfo = createDefaultDebugUtilsMessengerCreateInfo();
    // Layers
    if(enableValidationLayers)
    {
        createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        createInfo.ppEnabledLayerNames = validationLayers.data();

        createInfo.pNext = &debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    VkInstance instance;
    if(vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create vulkan instance");
    }

    return instance;
}

bool checkValidationLayerSupport(Span<const char* const> validationLayers)
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for(const auto& layerName : validationLayers)
    {
        bool layerFound = false;
        for(const auto& layerProperties : availableLayers)
        {
            if(strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }
        if(!layerFound)
        {
            return false;
        }
    }

    return true;
}

std::vector<const char*> getRequiredExtensions(bool enableValidationLayers)
{
    uint32_t availExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availExtensions(availExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtensionCount, availExtensions.data());
    std::cout << "Available extensions: \n";
    for(const auto& extension : availExtensions)
    {
        std::cout << "\t" << extension.extensionName << "\n";
    }

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if(enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VkDebugUtilsMessengerEXT setupDebugMessenger(VkInstance instance)
{
    VkDebugUtilsMessengerEXT debugMessenger;

    auto createInfo = createDefaultDebugUtilsMessengerCreateInfo();

    if(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }

    return debugMessenger;
}