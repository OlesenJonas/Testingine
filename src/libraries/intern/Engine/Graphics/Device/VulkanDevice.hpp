#pragma once

#include "BindlessManager.hpp"
#include "HelperTypes.hpp"
#include "VulkanConversions.hpp"
#include <Engine/Graphics/Graphics.hpp>
#include <Engine/Misc/Macros.hpp>

#include <Datastructures/FunctionQueue.hpp>
#include <Datastructures/Span.hpp>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;
struct Material;
struct Barrier;
struct Buffer;
struct ComputeShader;

class VulkanDevice
{
    CREATE_STATIC_GETTER(VulkanDevice);

  public:
    void init(GLFWwindow* window);

    uint32_t getSwapchainWidth();
    uint32_t getSwapchainHeight();

    void cleanup();

    // Dont really like these functions, but have to do for now until a framegraph is implemented
    void startNextFrame();
    VkCommandBuffer beginCommandBuffer();
    void endCommandBuffer(VkCommandBuffer cmd);

    // TODO: this currently functions as the sole submit for a whole frame
    //       but thats too conservative, not all command buffers may need to wait for
    //       the swapchain image to be available or even be submitted at the same time in the first place
    void submitCommandBuffers(Span<const VkCommandBuffer> cmdBuffers);

    // TODO: turn the functions into member functions of the command buffer instead ?

    void beginRendering(VkCommandBuffer cmd, Span<const RenderTarget>&& colorTargets, RenderTarget&& depthTarget);
    void endRendering(VkCommandBuffer cmd);
    void insertSwapchainImageBarrier(VkCommandBuffer cmd, ResourceState currentState, ResourceState targetState);
    void insertBarriers(VkCommandBuffer cmd, Span<const Barrier> barriers);

    void setGraphicsPipelineState(VkCommandBuffer cmd, Handle<Material> mat);
    void setComputePipelineState(VkCommandBuffer cmd, Handle<ComputeShader> shader);
    // this explicitely uses the bindless layout, so just call this function setBindlessIndices ??
    void pushConstants(VkCommandBuffer cmd, size_t size, void* data, size_t offset = 0);
    void bindIndexBuffer(VkCommandBuffer cmd, Handle<Buffer> buffer, size_t offset = 0);
    void bindVertexBuffers(
        VkCommandBuffer cmd,
        uint32_t startBinding,
        uint32_t count,
        Span<const Handle<Buffer>> buffers,
        Span<const uint64_t> offsets);
    void drawIndexed(
        VkCommandBuffer cmd,
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        uint32_t vertexOffset,
        uint32_t firstInstance);
    void drawImGui(VkCommandBuffer cmd);

    void dispatchCompute(VkCommandBuffer cmd, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);

    void presentSwapchain();

    void startDebugRegion(VkCommandBuffer cmd, const char* name);
    void endDebugRegion(VkCommandBuffer cmd);
    template <VulkanConvertible T>
    void setDebugName(T object, const char* name)
    {
        setDebugName(toVkObjectType<T>(), (uint64_t)object, name);
    }

    // Waits until all currently submitted GPU commands are executed
    void waitForWorkFinished();

    void fillMipLevels(VkCommandBuffer cmd, Handle<Texture> texture, ResourceState state);

    static constexpr int FRAMES_IN_FLIGHT = 2;

  private:
    struct PerFrameData
    {
        VkSemaphore swapchainImageAvailable = VK_NULL_HANDLE;
        VkSemaphore swapchainImageRenderFinished = VK_NULL_HANDLE;
        VkFence commandsDone = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> usedCommandBuffers;
    };
    PerFrameData perFrameData[FRAMES_IN_FLIGHT];
    inline PerFrameData& getCurrentFrameData()
    {
        return perFrameData[frameNumber % FRAMES_IN_FLIGHT];
    }
    uint32_t currentSwapchainImageIndex = 0xFFFFFFFF;

    // TODO: move into variable part on refactor
    //       maybe reference application instead, and just get window from application
    //       not sure if I want to cover scenario where window pointer can change in the future
    GLFWwindow* mainWindow;

    int frameNumber = -1;

    bool _initialized = false;

#ifdef ENABLE_VULKAN_VALIDATION
    bool enableValidationLayers = true;
#else
    bool enableValidationLayers = false;
#endif

    // TODO: remove need to access this from the outside!
  public:
    VkDevice device = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;

    VmaAllocator allocator;

    BindlessManager bindlessManager{*this};
    // this is here because it felt out of place inside the manager
    VkPipelineLayout bindlessPipelineLayout;

    // TODO: not sure yet what to do about this
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    FunctionQueue<> deleteQueue;

    VkPipelineCache pipelineCache;
    VkFormat swapchainImageFormat;
    VkFormat defaultDepthFormat;

  private:
    // ---------
    VkDebugUtilsMessengerEXT debugMessenger;
    PFN_vkSetDebugUtilsObjectNameEXT setObjectDebugName = nullptr;

    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDeviceProperties;

    QueueFamilyIndices queueFamilyIndices;

    VkSwapchainKHR swapchain;
    // On high-dpi monitors, swapChainExtent is not necessarily window extent!
    VkExtent2D swapchainExtent;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkQueue graphicsAndComputeQueue;
    uint32_t graphicsAndComputeQueueFamily;

    const std::string_view pipelineCacheFile = "vkpipelinecache";

    struct UploadContext
    {
        VkFence uploadFence;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
    };
    UploadContext uploadContext;

    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initSyncStructures();
    void initBindless();
    void initPipelineCache();
    void initImGui();

    void savePipelineCache();

    size_t padUniformBufferSize(size_t originalSize);

    void setDebugName(VkObjectType type, uint64_t handle, const char* name);
};

extern PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT;