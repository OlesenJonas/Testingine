#pragma once

#include <vulkan/vulkan.h>

#include "Datastructures/Span.hpp"

#include <optional>
#include <vector>

VkInstance createInstance(
    bool enableValidationLayers,
    Span<const char* const> validationLayers,
    Span<const char* const> requiredExtensions);

bool checkValidationLayerSupport(Span<const char* const> validationLayers);

std::vector<const char*> getRequiredSurfaceExtensions(bool enableValidationLayers);

VkDebugUtilsMessengerEXT setupDebugMessenger(VkInstance instance, bool* breakOnError);