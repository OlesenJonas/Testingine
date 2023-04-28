#pragma once

#include <VMA/VMA.hpp>
#include <string_view>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

#include "../Buffer/Buffer.hpp"
#include "../RenderObject/RenderObject.hpp"
#include "../Texture/Texture.hpp"
#include "../VulkanTypes.hpp"
#include "BindlessManager.hpp"

#include <intern/Datastructures/FunctionQueue.hpp>
#include <intern/Misc/Macros.hpp>

#include <array>
#include <string>
#include <unordered_map>

struct GPUCameraData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 projView;
};

struct GPUObjectData
{
    glm::mat4 modelMatrix;
};

class VulkanRenderer
{
  public:
    CREATE_STATIC_GETTER(VulkanRenderer);

    void init();
    void cleanup();

    void draw();

    static constexpr int FRAMES_IN_FLIGHT = 2;

    bool isInitialized = false;

    int frameNumber = 0;

#ifdef ENABLE_VULKAN_VALIDATION
    bool enableValidationLayers = true;
#else
    bool enableValidationLayers = false;
#endif
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkDevice device = VK_NULL_HANDLE;

    QueueFamilyIndices queueFamilyIndices;

    VkSwapchainKHR swapchain;
    // On high-dpi monitors, swapChainExtent is not necessarily window extent!
    VkExtent2D swapchainExtent;
    VkFormat swapchainImageFormat;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    Handle<Texture> depthTexture;

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VkPipelineCache pipelineCache;
    const std::string_view pipelineCacheFile = "vkpipelinecache";

    struct FrameData
    {
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence renderFence;
        VkCommandPool commandPool;
        // todo: one command buffer for offscreen and one for present (at least)
        VkCommandBuffer mainCommandBuffer;

        Handle<Buffer> cameraBuffer;
        Handle<Buffer> objectBuffer;
    };
    FrameData perFrameData[FRAMES_IN_FLIGHT];
    inline FrameData& getCurrentFrameData()
    {
        return perFrameData[frameNumber % FRAMES_IN_FLIGHT];
    }

    struct UploadContext
    {
        VkFence uploadFence;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
    };
    UploadContext uploadContext;

    FunctionQueue<> deleteQueue;

    VmaAllocator allocator;

    BindlessManager bindlessManager{*this};
    // this is here because it felt out of place inside the manager
    VkPipelineLayout bindlessPipelineLayout;
    struct BindlessIndices
    {
        // Frame globals
        uint32_t FrameDataBuffer;
        // Resolution, matrices (differs in eg. shadow and default pass)
        uint32_t RenderInfoBuffer;
        // Buffer with object transforms and index into that buffer
        uint32_t transformBuffer;
        uint32_t transformIndex;
        // Buffer with material/-instance parameters
        uint32_t materialParamsBuffer;
        uint32_t materialInstanceParamsBuffer;
    };

    std::vector<RenderObject> renderables;

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    // Waits until all currently submitted GPU commands are executed
    void waitForWorkFinished();

    void drawObjects(VkCommandBuffer cmd, RenderObject* first, int count);

  private:
    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initSyncStructures();
    void initBindless();
    void initPipelineCache();
    void initImGui();
    void initGlobalBuffers();

    void savePipelineCache();

    size_t padUniformBufferSize(size_t originalSize);
};