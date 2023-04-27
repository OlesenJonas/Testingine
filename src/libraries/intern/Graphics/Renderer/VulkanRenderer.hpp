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

#include <array>
#include <string>
#include <unordered_map>

// TODO: dont like this being here
struct MeshPushConstants
{
    glm::vec4 data;
    glm::mat4 transformMatrix;
};

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

struct UploadContext
{
    VkFence uploadFence;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
};

struct FrameData
{
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence renderFence;
    // todo: one command buffer for offscreen and one for present
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    Handle<Buffer> cameraBuffer;
    VkDescriptorSet globalDescriptor;

    Handle<Buffer> objectBuffer;
    VkDescriptorSet objectDescriptor;
};

class VulkanRenderer
{
  public:
    [[nodiscard]] static inline VulkanRenderer* get()
    {
        return ptr;
    }

    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw function
    void draw();

    static constexpr int FRAMES_IN_FLIGHT = 2;

    bool isInitialized{false};

    int frameNumber{0};

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
    // VkImageView depthImageView;
    // AllocatedImage depthImage;

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VkPipelineCache pipelineCache;
    const std::string_view pipelineCacheFile = "vkpipelinecache";

    FrameData perFrameData[FRAMES_IN_FLIGHT];
    inline FrameData& getCurrentFrameData()
    {
        return perFrameData[frameNumber % FRAMES_IN_FLIGHT];
    }
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

    // Wait until all currently submitted GPU commands are executed
    void waitForWorkFinished();

    // TODO: refactor to take span
    void drawObjects(VkCommandBuffer cmd, RenderObject* first, int count);

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

  private:
    inline static VulkanRenderer* ptr = nullptr;

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