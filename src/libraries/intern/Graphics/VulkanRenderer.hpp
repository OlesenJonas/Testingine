#pragma once

#include <VMA/VMA.hpp>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

#include "Material.hpp"
#include "Mesh.hpp"
#include "RenderObject.hpp"
#include "Texture.hpp"
#include "VulkanTypes.hpp"

#include <intern/Datastructures/FunctionQueue.hpp>

#include <string>
#include <unordered_map>

struct GLFWwindow;

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
    // TODO: rename into imageAvailable, renderFinished
    VkSemaphore presentSemaphore;
    VkSemaphore renderSemaphore;
    VkFence renderFence;
    // todo: one command buffer for offscreen and one for present
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    AllocatedBuffer cameraBuffer;
    VkDescriptorSet globalDescriptor;

    AllocatedBuffer objectBuffer;
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
    GLFWwindow* window = nullptr;
    VkExtent2D windowExtent{1700, 900};

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

    VkImageView depthImageView;
    AllocatedImage depthImage;
    VkFormat depthFormat;

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

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
    AllocatedBuffer sceneParameterBuffer; // Holds FRAMES_IN_FLIGHT * aligned(GPUSceneData)

    FunctionQueue<> deleteQueue;

    VmaAllocator allocator;

    std::vector<RenderObject> renderables;
    std::unordered_map<std::string, Material> materials;
    std::unordered_map<std::string, Mesh> meshes;
    std::unordered_map<std::string, Texture> loadedTextures;

    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
    Material* getMaterial(const std::string& name);
    Mesh* getMesh(const std::string& name);

    // TODO: refactor to take span
    void drawObjects(VkCommandBuffer cmd, RenderObject* first, int count);

    AllocatedBuffer createBuffer(
        size_t allocSize,
        VkBufferUsageFlags usage,
        VmaAllocationCreateFlags flags,
        VkMemoryPropertyFlags requiredFlags);

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

  private:
    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initDefaultRenderpass();
    void initFramebuffers();
    void initSyncStructures();
    void initDescriptors();
    void initPipelines();

    void initImGui();

    void loadMeshes();
    void uploadMesh(Mesh& mesh);
    void loadImages();

    void initScene();

    bool loadShaderModule(const char* filePath, VkShaderModule* outShaderModule);

    size_t padUniformBufferSize(size_t originalSize);
};