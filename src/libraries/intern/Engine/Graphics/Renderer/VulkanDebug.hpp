#pragma once

#include <string_view>
#include <vulkan/vulkan.h>

#ifdef NDEBUG
    #define assertVkResult(x) x
#else
    #include <cassert>
    #define assertVkResult(x) assert(x == VK_SUCCESS);
#endif

VkDebugUtilsMessengerCreateInfoEXT createDefaultDebugUtilsMessengerCreateInfo();

VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

void setDebugName(VkBuffer buffer, const char* name);
void setDebugName(VkImage image, const char* name);
void setDebugName(VkImageView imageView, const char* name);
void setDebugName(VkShaderModule shader, const char* name);
void setDebugName(VkDescriptorSet set, const char* name);
void setDebugName(VkDescriptorSetLayout setLayout, const char* name);
void setDebugName(VkPipeline pipeline, const char* name);

extern PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT;