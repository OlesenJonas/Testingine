#pragma once

#include <VMA/VMA.hpp>
#include <string_view>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

#include "../Buffer/Buffer.hpp"
#include "../RenderObject/RenderObject.hpp"
#include "../Texture/Texture.hpp"
#include "../VulkanTypes.hpp"

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

    // ----------------

    // TODO:
    //   Factor out all the bindless stuff
    //   Put inside the resourceManager? Dont like that. A new bindless Manager?

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

    //  todo: dont like the name
    struct BindlessDescriptorsInfo
    {
        uint32_t setIndex = 0xFFFFFFFF;
        uint32_t limit = 0xFFFFFFFF;
        std::string_view debugName;
        uint32_t freeIndex = 0;
    };
    // todo: check against limit from physicalDeviceProperties.limits
    std::unordered_map<VkDescriptorType, BindlessDescriptorsInfo> descriptorTypeTable = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {0, 128, "SampledImages"}},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, {1, 128, "StorageImages"}},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {2, 128, "UniformBuffers"}},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, {3, 128, "StorageBuffers"}},
    };

    // todo: dont like this being a vector, the size needs to be const!
    const uint32_t immutableSamplerCount = 1;
    std::vector<VkSampler> immutableSamplers;

    VkDescriptorPool bindlessDescriptorPool;
    std::array<VkDescriptorSetLayout, 4> bindlessSetLayouts;
    std::array<VkDescriptorSet, 4> bindlessDescriptorSets;
    VkPipelineLayout bindlessPipelineLayout;

    uint32_t createUniformBufferBinding(VkBuffer buffer);
    uint32_t createStorageBufferBinding(VkBuffer buffer);
    uint32_t createSampledImageBinding(VkImageView view, VkImageLayout layout);
    uint32_t createStorageImageBinding(VkImageView view, VkImageLayout layout);

    // ----------------

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
    void setupBindless();
    void initPipelineCache();
    void initImGui();
    void initGlobalBuffers();

    void savePipelineCache();

    size_t padUniformBufferSize(size_t originalSize);
};