#pragma once

#include "VulkanTypes.hpp"
#include <vulkan/vulkan.h>

#include "Datastructures/Span.hpp"

#include <optional>
#include <vector>

VkInstance createInstance(bool enableValidationLayers, Span<const char* const> validationLayers);

bool checkValidationLayerSupport(Span<const char* const> validationLayers);

std::vector<const char*> getRequiredExtensions(bool enableValidationLayers);

VkDebugUtilsMessengerEXT setupDebugMessenger(VkInstance instance);