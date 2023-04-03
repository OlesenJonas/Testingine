#pragma once

#include <VMA/VMA.hpp>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

#include "../Material/Material.hpp"
#include "../Mesh/Mesh.hpp"
#include "../RenderObject/RenderObject.hpp"
#include "../Texture/Texture.hpp"
#include "../VulkanTypes.hpp"

#include <intern/Datastructures/FunctionQueue.hpp>

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

struct GPUSceneData
{
    glm::vec4 fogColor;
    glm::vec4 fogDistances;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
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

constexpr int FRAMES_IN_FLIGHT = 2;

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
    VkDevice device;
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

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorSetLayout objectSetLayout;
    VkDescriptorSetLayout singleTextureSetLayout;

    FrameData frames[FRAMES_IN_FLIGHT];
    inline FrameData& getCurrentFrameData()
    {
        return frames[frameNumber % FRAMES_IN_FLIGHT];
    }
    UploadContext uploadContext;

    GPUSceneData sceneParameters;
    Handle<Buffer> sceneParameterBuffer; // Holds FRAMES_IN_FLIGHT * aligned(GPUSceneData)

    FunctionQueue<> deleteQueue;

    VmaAllocator allocator;

    std::vector<RenderObject> renderables;

    // TODO: refactor to take span
    void drawObjects(VkCommandBuffer cmd, RenderObject* first, int count);

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

  private:
    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initSyncStructures();
    void initDescriptors();
    void initPipelines();

    void initImGui();

    void loadMeshes();
    // void uploadMesh(Mesh& mesh);
    void loadImages();

    void initScene();

    bool loadShaderModule(const char* filePath, VkShaderModule* outShaderModule);

    size_t padUniformBufferSize(size_t originalSize);
};