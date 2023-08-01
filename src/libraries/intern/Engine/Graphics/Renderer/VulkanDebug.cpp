#include "VulkanDebug.hpp"

#include <Engine/Graphics/Renderer/VulkanRenderer.hpp>
#include <Engine/Misc/Macros.hpp>
#include <iostream>
#include <vulkan/vulkan_core.h>

VkDebugUtilsMessengerCreateInfoEXT createDefaultDebugUtilsMessengerCreateInfo()
{
    return {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = defaultDebugCallback,
        .pUserData = nullptr};
}

VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    (void)messageType;
    (void)pCallbackData;
    (void)pUserData;
    std::cerr << "validation layer: " << pCallbackData->pMessage << "\n" << std::endl;
    if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        BREAKPOINT;
    }
    // if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
    // if(messageType != VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT){
    //}

    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if(func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(

    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if(func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

PFN_vkSetDebugUtilsObjectNameEXT setObjectDebugName = nullptr;

void setDebugName(VkObjectType type, uint64_t handle, const char* name)
{
    if(setObjectDebugName == nullptr)
    {
        setObjectDebugName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
            VulkanRenderer::get()->instance, "vkSetDebugUtilsObjectNameEXT");
        assert(setObjectDebugName);
    }
    VkDebugUtilsObjectNameInfoEXT nameInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = type,
        .objectHandle = handle,
        .pObjectName = name,
    };
    setObjectDebugName(VulkanRenderer::get()->device, &nameInfo);
}

void setDebugName(VkBuffer buffer, const char* name)
{
    setDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, name);
}
void setDebugName(VkImage image, const char* name)
{
    setDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)image, name);
}
void setDebugName(VkImageView imageView, const char* name)
{
    setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)imageView, name);
}
void setDebugName(VkShaderModule shader, const char* name)
{
    setDebugName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shader, name);
}
void setDebugName(VkDescriptorSet set, const char* name)
{
    setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)set, name);
}
void setDebugName(VkDescriptorSetLayout setLayout, const char* name)
{
    setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)setLayout, name);
}
void setDebugName(VkPipeline pipeline, const char* name)
{
    setDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, name);
}