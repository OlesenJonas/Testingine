#include "VulkanDeviceFinder.hpp"
#include "Graphics/VulkanDeviceFinder.hpp"
#include "Graphics/VulkanTypes.hpp"

#include <set>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

VulkanDeviceFinder::VulkanDeviceFinder(VkInstance instance) : instance(instance)
{
}

void VulkanDeviceFinder::setSurface(VkSurfaceKHR surface)
{
    this->surface = surface;
}
void VulkanDeviceFinder::setValidationLayers(Span<const char* const> validationLayers)
{
    enableValidationLayers = true;
    this->validationLayers = std::vector<const char*>(validationLayers.begin(), validationLayers.end());
}
void VulkanDeviceFinder::setExtensions(Span<const char* const> deviceExtensions)
{
    this->deviceExtensions = std::vector<const char*>(deviceExtensions.begin(), deviceExtensions.end());
}

VkPhysicalDevice VulkanDeviceFinder::findPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if(deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for(const auto& device : devices)
    {
        if(isDeviceSuitable(device))
        {
            physicalDevice = device;
            break;
        }
    }

    if(physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU");
    }

    return physicalDevice;
}

VkDevice VulkanDeviceFinder::createLogicalDevice()
{
    queueFamilyIndices = findQueueFamilies(physicalDevice);

    std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value()};

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for(uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    // Shader Draw Parameters are always needed
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParamFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES,
        .pNext = nullptr,
        .shaderDrawParameters = VK_TRUE,
    };

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &shaderDrawParamFeatures,
    };
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if(enableValidationLayers)
    {
        createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device");
    }

    return device;
}

QueueFamilyIndices VulkanDeviceFinder::getQueueFamilyIndices()
{
    return queueFamilyIndices;
}