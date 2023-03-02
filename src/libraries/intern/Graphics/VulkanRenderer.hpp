#pragma once

#include <VMA/VMA.hpp>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

#include "Mesh.hpp"
#include "VulkanTypes.hpp"

#include <intern/Datastructures/FunctionQueue.hpp>

struct GLFWwindow;

// TODO: dont like this being here
struct MeshPushConstants
{
    glm::vec4 data;
    glm::mat4 transformMatrix;
};

class VulkanRenderer
{
  public:
    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw function
    void draw();

    // run main loop
    void run();

    bool isInitialized{false};

    int frameNumber{0};
    GLFWwindow* window = nullptr;
    VkExtent2D windowExtent{1700, 900};

#ifdef NDEBUG
    bool enableValidationLayers = false;
#else
    bool enableValidationLayers = true;
#endif
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    QueueFamilyIndices queueFamilyIndices;

    VkSwapchainKHR swapchain;
    // On high-dpi monitors, swapChainExtent is not necessarily window extent!
    VkExtent2D swapchainExtent;
    VkFormat swapchainImageFormat;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    // todo: rename to imageAvailable and renderFinished, think those names are clearer
    VkSemaphore presentSemaphore;
    VkSemaphore renderSemaphore;
    VkFence renderFence;

    // Dont like this being here, would fit better in VulkanPipeline I guess
    VkPipelineLayout trianglePipelineLayout;
    VkPipeline trianglePipeline;

    FunctionQueue<> deleteQueue;

    VmaAllocator allocator;

    VkPipelineLayout meshPipelineLayout;
    VkPipeline meshPipeline;
    Mesh triangleMesh;

  private:
    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initDefaultRenderpass();
    void initFramebuffers();
    void initSyncStructures();
    void initPipelines();

    void loadMeshes();
    void uploadMesh(Mesh& mesh);

    bool loadShaderModule(const char* filePath, VkShaderModule* outShaderModule);
};