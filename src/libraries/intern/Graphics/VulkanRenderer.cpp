#include "VulkanRenderer.hpp"
#include "VulkanDebug.hpp"
#include "VulkanDeviceFinder.hpp"
#include "VulkanInit.hpp"
#include "VulkanSwapchainSetup.hpp"
#include "VulkanTypes.hpp"

#include "Datastructures/Span.hpp"

#include <stdexcept>
#include <string>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

void VulkanRenderer::init()
{
    // Init window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(windowExtent.width, windowExtent.height, "Vulkan Test", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);

    initVulkan();

    initSwapchain();

    // everything went fine
    isInitialized = true;
}

void VulkanRenderer::initVulkan()
{
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    instance = createInstance(enableValidationLayers, validationLayers);

    if(enableValidationLayers)
        debugMessenger = setupDebugMessenger(instance);

    if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    VulkanDeviceFinder deviceFinder(instance);
    deviceFinder.setSurface(surface);
    if(enableValidationLayers)
        deviceFinder.setValidationLayers(validationLayers);
    deviceFinder.setExtensions(deviceExtensions);

    physicalDevice = deviceFinder.findPhysicalDevice();
    device = deviceFinder.createLogicalDevice();
    queueFamilyIndices = deviceFinder.getQueueFamilyIndices();

    // vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    // vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanRenderer::initSwapchain()
{
    VulkanSwapchainSetup swapchainSetup(physicalDevice, device, window, surface);
    swapchainSetup.setup(queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value());

    swapchain = swapchainSetup.getSwapchain();
    swapchainExtent = swapchainSetup.getSwapchainExtent();
    swapchainImageFormat = swapchainSetup.getSwapchainImageFormat();
    swapchainImages = swapchainSetup.getSwapchainImages();
    swapchainImageViews = swapchainSetup.createSwapchainImageViews();
}

void VulkanRenderer::cleanup()
{
    if(!isInitialized)
        return;

    for(int i = 0; i < swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    vkDestroyDevice(device, nullptr);

    if(enableValidationLayers)
    {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}

void VulkanRenderer::run()
{
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        draw();
    }

    vkDeviceWaitIdle(device);
}

void VulkanRenderer::draw()
{
    // nothing yet
}
