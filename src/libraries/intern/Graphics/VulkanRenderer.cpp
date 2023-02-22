#include "VulkanRenderer.hpp"
#include "VulkanInit.hpp"
#include "VulkanTypes.hpp"

#include "Datastructures/Span.hpp"

#include <string>

#include <GLFW/glfw3.h>

void VulkanRenderer::init()
{
    // Init window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(windowExtent.width, windowExtent.height, "Vulkan Test", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);

    initVulkan();

    // everything went fine
    isInitialized = true;
}

void VulkanRenderer::initVulkan()
{
    instance = createInstance(enableValidationLayers, {"VK_LAYER_KHRONOS_validation"});

    if(enableValidationLayers)
        debugMessenger = setupDebugMessenger(instance);
}

void VulkanRenderer::cleanup()
{
    if(isInitialized)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

void VulkanRenderer::run()
{
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        draw();
    }

    // vkDeviceWaitIdle(device);
}

void VulkanRenderer::draw()
{
    // nothing yet
}
