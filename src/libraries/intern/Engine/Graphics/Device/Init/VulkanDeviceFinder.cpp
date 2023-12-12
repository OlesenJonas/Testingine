#include "VulkanDeviceFinder.hpp"
#include "../HelperTypes.hpp"

#include <set>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

VulkanDeviceFinder::VulkanDeviceFinder(VkInstance instance) : instance(instance) {}

void VulkanDeviceFinder::setSurface(VkSurfaceKHR surface) { this->surface = surface; }
void VulkanDeviceFinder::setValidationLayers(Span<const char* const> validationLayers)
{
    enableValidationLayers = true;
    this->validationLayers = validationLayers;
}
void VulkanDeviceFinder::setExtensions(Span<const char* const> deviceExtensions)
{
    this->deviceExtensions = deviceExtensions;
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
        queueFamilyIndices.graphicsAndComputeFamily.value(), queueFamilyIndices.presentFamily.value()};
    // TODO: handle present and graphics/compute queue not being part of the same family!
    assert(uniqueQueueFamilies.size() == 1);

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

    // TODO: if the amount of requested core features grows, switch to
    //   https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceVulkan13Features.html
    //   and similar Vulkan11-/Vulkan12Features instead of using all these individual structs

    VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
        .pNext = nullptr,
        .scalarBlockLayout = VK_TRUE,
    };

    VkPhysicalDeviceDescriptorIndexingFeatures descIndexingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &scalarBlockLayoutFeatures,
        .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
        .shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE,
        .shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE,
        .shaderUniformBufferArrayNonUniformIndexing = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
        .shaderStorageImageArrayNonUniformIndexing = VK_TRUE,
        .shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
        .shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE,
        .shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
    };

    VkPhysicalDeviceSynchronization2Features synch2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext = &descIndexingFeatures,
        .synchronization2 = VK_TRUE,
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &synch2Features,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParamFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES,
        .pNext = &dynamicRenderingFeatures,
        .shaderDrawParameters = VK_TRUE,
    };

    VkPhysicalDeviceMaintenance4Features maint4Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
        .pNext = &shaderDrawParamFeatures,
        .maintenance4 = VK_TRUE,
    };

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShadingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .pNext = &maint4Features,
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
        //...
    };

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &meshShadingFeatures,
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

QueueFamilyIndices VulkanDeviceFinder::getQueueFamilyIndices() { return queueFamilyIndices; }
bool VulkanDeviceFinder::isDeviceSuitable(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    bool featuresSupported = true;
    {
        VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
            .pNext = nullptr,
        };
        VkPhysicalDeviceDescriptorIndexingFeatures descIndexingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
            .pNext = &scalarBlockLayoutFeatures,
        };
        VkPhysicalDeviceSynchronization2Features synch2Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .pNext = &descIndexingFeatures,
        };
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = &synch2Features,
        };
        VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParamFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES,
            .pNext = &dynamicRenderingFeatures,
        };
        //--
        VkPhysicalDeviceFeatures2 deviceFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &shaderDrawParamFeatures,
        };
        vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);
        featuresSupported &= scalarBlockLayoutFeatures.scalarBlockLayout;

        featuresSupported &= descIndexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing;
        featuresSupported &= descIndexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing;
        featuresSupported &= descIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing;
        featuresSupported &= descIndexingFeatures.shaderSampledImageArrayNonUniformIndexing;
        featuresSupported &= descIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing;
        featuresSupported &= descIndexingFeatures.shaderStorageImageArrayNonUniformIndexing;
        featuresSupported &= descIndexingFeatures.shaderUniformTexelBufferArrayNonUniformIndexing;
        featuresSupported &= descIndexingFeatures.shaderStorageTexelBufferArrayNonUniformIndexing;
        featuresSupported &= descIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind;
        featuresSupported &= descIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind;
        featuresSupported &= descIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind;
        featuresSupported &= descIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind;
        featuresSupported &= descIndexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind;
        featuresSupported &= descIndexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind;
        featuresSupported &= descIndexingFeatures.descriptorBindingUpdateUnusedWhilePending;
        featuresSupported &= descIndexingFeatures.descriptorBindingPartiallyBound;
        featuresSupported &= descIndexingFeatures.descriptorBindingVariableDescriptorCount;
        featuresSupported &= descIndexingFeatures.runtimeDescriptorArray;

        featuresSupported &= synch2Features.synchronization2;

        featuresSupported &= dynamicRenderingFeatures.dynamicRendering;
        featuresSupported &= shaderDrawParamFeatures.shaderDrawParameters;

        // TODO: also check mesh shading support!
    }

    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if(extensionsSupported)
    {
        SwapchainSupportDetails swapChainSupport = querySwapchainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    auto queueIndices = findQueueFamilies(device);

    return extensionsSupported && featuresSupported && queueIndices.isComplete() && swapChainAdequate &&
           (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
}

bool VulkanDeviceFinder::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for(const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapchainSupportDetails VulkanDeviceFinder::querySwapchainSupport(VkPhysicalDevice device)
{
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if(formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if(presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

QueueFamilyIndices VulkanDeviceFinder::findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for(int i = 0; i < queueFamilies.size(); i++)
    {
        if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        if((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
           (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            indices.graphicsAndComputeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if(presentSupport)
        {
            indices.presentFamily = i;
        }

        if(indices.isComplete())
        {
            break;
        }
    }

    return indices;
}