#include "VulkanRenderer.hpp"
#include "../Barriers/Barrier.hpp"
#include "../Mesh/Mesh.hpp"
#include "../RenderObject/RenderObject.hpp"
#include "../Texture/Texture.hpp"
#include "../VulkanTypes.hpp"
#include "Graphics/Pipeline/GraphicsPipeline.hpp"
#include "Graphics/Renderer/VulkanDebug.hpp"
#include "Graphics/Renderer/VulkanRenderer.hpp"
#include "Init/VulkanDeviceFinder.hpp"
#include "Init/VulkanInit.hpp"
#include "Init/VulkanSwapchainSetup.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "VulkanDebug.hpp"

#include <intern/Datastructures/Span.hpp>
#include <intern/Engine/Engine.hpp>

#include <fstream>
#include <iostream>
#include <shaderc/shaderc.h>
#include <stdexcept>
#include <string>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_vulkan.h"
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/trigonometric.hpp>
#include <vulkan/vulkan_core.h>

void VulkanRenderer::init()
{
    ptr = this;

    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();
    initDescriptors();
    initImGui();

    initGlobalDescriptorSets();
    initPipelines();

    // everything went fine
    isInitialized = true;
}

void VulkanRenderer::initVulkan()
{
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
    const std::vector<const char*> instanceExtensions = {
        // Need this for debug markers, not just for layers
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    instance = createInstance(enableValidationLayers, validationLayers, instanceExtensions);

    if(enableValidationLayers)
        debugMessenger = setupDebugMessenger(instance);

    if(glfwCreateWindowSurface(instance, Engine::get()->getMainWindow()->glfwWindow, nullptr, &surface) !=
       VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VulkanDeviceFinder deviceFinder(instance);
    deviceFinder.setSurface(surface);
    if(enableValidationLayers)
        deviceFinder.setValidationLayers(validationLayers);
    deviceFinder.setExtensions(deviceExtensions);

    physicalDevice = deviceFinder.findPhysicalDevice();

    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    device = deviceFinder.createLogicalDevice();

    queueFamilyIndices = deviceFinder.getQueueFamilyIndices();
    graphicsQueueFamily = queueFamilyIndices.graphicsFamily.value();

    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    // vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    VmaAllocatorCreateInfo vmaAllocatorCrInfo{
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
    };
    vmaCreateAllocator(&vmaAllocatorCrInfo, &allocator);
}

void VulkanRenderer::initSwapchain()
{
    VulkanSwapchainSetup swapchainSetup(
        physicalDevice, device, Engine::get()->getMainWindow()->glfwWindow, surface);
    swapchainSetup.setup(queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value());

    swapchain = swapchainSetup.getSwapchain();
    swapchainExtent = swapchainSetup.getSwapchainExtent();
    swapchainImageFormat = swapchainSetup.getSwapchainImageFormat();
    swapchainImages = swapchainSetup.getSwapchainImages();

    swapchainImageViews = swapchainSetup.createSwapchainImageViews();

    ResourceManager& rsrcManager = *Engine::get()->getResourceManager();
    depthTexture = rsrcManager.createTexture(
        Texture::Info{
            .size = {swapchainExtent.width, swapchainExtent.height, 1},
            .format = depthFormat,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .viewAspect = VK_IMAGE_ASPECT_DEPTH_BIT,
        },
        "Depth texture");

    deleteQueue.pushBack([=]() { vkDestroySwapchainKHR(device, swapchain, nullptr); });
    for(VkImageView view : swapchainImageViews)
    {
        deleteQueue.pushBack([=]() { vkDestroyImageView(device, view, nullptr); });
    }
}

void VulkanRenderer::initCommands()
{
    VkCommandPoolCreateInfo commandPoolCrInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,

        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueFamily,
    };

    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        assertVkResult(vkCreateCommandPool(device, &commandPoolCrInfo, nullptr, &frames[i].commandPool));

        VkCommandBufferAllocateInfo cmdBuffAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,

            .commandPool = frames[i].commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        assertVkResult(vkAllocateCommandBuffers(device, &cmdBuffAllocInfo, &frames[i].mainCommandBuffer));

        deleteQueue.pushBack([=]() { vkDestroyCommandPool(device, frames[i].commandPool, nullptr); });
    }

    VkCommandPoolCreateInfo uploadCommandPoolCrInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .queueFamilyIndex = graphicsQueueFamily,
    };
    assertVkResult(vkCreateCommandPool(device, &uploadCommandPoolCrInfo, nullptr, &uploadContext.commandPool));
    deleteQueue.pushBack([=]() { vkDestroyCommandPool(device, uploadContext.commandPool, nullptr); });

    VkCommandBufferAllocateInfo cmdBuffAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,

        .commandPool = uploadContext.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    assertVkResult(vkAllocateCommandBuffers(device, &cmdBuffAllocInfo, &uploadContext.commandBuffer));
}

void VulkanRenderer::initSyncStructures()
{
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,

        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };

    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        assertVkResult(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));

        deleteQueue.pushBack([=]() { vkDestroyFence(device, frames[i].renderFence, nullptr); });

        assertVkResult(
            vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].imageAvailableSemaphore));
        assertVkResult(
            vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderFinishedSemaphore));

        deleteQueue.pushBack(
            [=]()
            {
                vkDestroySemaphore(device, frames[i].imageAvailableSemaphore, nullptr);
                vkDestroySemaphore(device, frames[i].renderFinishedSemaphore, nullptr);
            });
    }

    VkFenceCreateInfo uploadFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
    };
    assertVkResult(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadContext.uploadFence));
    deleteQueue.pushBack([=]() { vkDestroyFence(device, uploadContext.uploadFence, nullptr); });
}

void VulkanRenderer::initDescriptors()
{
    std::vector<VkDescriptorPoolSize> sizes = {
        {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 128},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = 128},
    };
    VkDescriptorPoolCreateInfo poolCrInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .maxSets = 128,
        .poolSizeCount = (uint32_t)sizes.size(),
        .pPoolSizes = sizes.data(),
    };
    assertVkResult(vkCreateDescriptorPool(device, &poolCrInfo, nullptr, &descriptorPool));

    deleteQueue.pushBack([=]() { vkDestroyDescriptorPool(device, descriptorPool, nullptr); });
}

void VulkanRenderer::initImGui()
{
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    VkDescriptorPoolCreateInfo descrPoolCrInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = std::size(poolSizes),
        .pPoolSizes = poolSizes,
    };

    VkDescriptorPool imguiPool;
    assertVkResult(vkCreateDescriptorPool(device, &descrPoolCrInfo, nullptr, &imguiPool));

    // 2: initialize the library
    ImGui::CreateContext();
    // init imgui for Glfw
    ImGui_ImplGlfw_InitForVulkan(Engine::get()->getMainWindow()->glfwWindow, true);
    // init imgui for Vulkan
    ImGui_ImplVulkan_InitInfo initInfo = {
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,
        .QueueFamily = graphicsQueueFamily,
        .Queue = graphicsQueue,
        .DescriptorPool = imguiPool,
        // todo: dont hardcode these, retrieve from swapchain creation
        .MinImageCount = 2,
        .ImageCount = 3,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    };

    ImGui_ImplVulkan_Init(&initInfo, swapchainImageFormat);

    // upload imgui font textures
    immediateSubmit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });
    // clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    // add imgui stuff to deletion queue
    deleteQueue.pushBack(
        [=]()
        {
            vkDestroyDescriptorPool(device, imguiPool, nullptr);
            ImGui_ImplVulkan_Shutdown();
        });
}

void VulkanRenderer::initGlobalDescriptorSets()
{
    // TODO: this no longer works since not all buffers may be used in all shader stages
    //       so have to figure out how to bind just the correct ones etc.
    //       But should be doable with the existing look up map
    VkDescriptorSetLayoutBinding camBufferBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };
    VkDescriptorSetLayoutBinding sceneParamsBufferBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        // .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr,
    };
    VkDescriptorSetLayoutBinding textureBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr,
    };
    VkDescriptorSetLayoutBinding bindings[] = {camBufferBinding, sceneParamsBufferBinding};
    VkDescriptorSetLayoutCreateInfo descrSetCrInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .bindingCount = 2,
        .pBindings = bindings,
    };
    vkCreateDescriptorSetLayout(device, &descrSetCrInfo, nullptr, &globalSetLayout);

    // object buffer
    VkDescriptorSetLayoutBinding objectBufferBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };
    VkDescriptorSetLayoutCreateInfo objectDescrSetCrInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .bindingCount = 1,
        .pBindings = &objectBufferBinding,
    };
    vkCreateDescriptorSetLayout(device, &objectDescrSetCrInfo, nullptr, &objectSetLayout);

    VkDescriptorSetLayoutCreateInfo textureDescrSetLayoutCrInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .bindingCount = 1,
        .pBindings = &textureBinding,
    };
    vkCreateDescriptorSetLayout(device, &textureDescrSetLayoutCrInfo, nullptr, &singleTextureSetLayout);

    const size_t sceneParamBufferSize = FRAMES_IN_FLIGHT * padUniformBufferSize(sizeof(GPUSceneData));
    // pretty sure this is overkill (combining Vma and Vk flags, but better safe than sorry )
    auto& rsrcManager = *Engine::get()->getResourceManager();
    sceneParameterBuffer = rsrcManager.createBuffer(
        Buffer::CreateInfo{
            .info =
                {
                    .size = sceneParamBufferSize,
                    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    .memoryAllocationInfo =
                        {
                            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
                            .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        },
                },
        },
        "sceneParameterBuffer");

    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        frames[i].cameraBuffer = rsrcManager.createBuffer(Buffer::CreateInfo{
            .info =
                {
                    .size = sizeof(GPUCameraData),
                    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    .memoryAllocationInfo =
                        {
                            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
                            .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        },
                },
        });

        const int MAX_OBJECTS = 10000;
        frames[i].objectBuffer = rsrcManager.createBuffer(Buffer::CreateInfo{
            .info =
                {
                    .size = sizeof(GPUObjectData) * MAX_OBJECTS,
                    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    .memoryAllocationInfo =
                        {
                            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
                            .requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        },
                },
        });

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,

            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &globalSetLayout,
        };
        vkAllocateDescriptorSets(device, &allocInfo, &frames[i].globalDescriptor);

        VkDescriptorBufferInfo camBufferInfo{
            .buffer = rsrcManager.get(frames[i].cameraBuffer)->buffer,
            .offset = 0,
            .range = sizeof(GPUCameraData),
        };

        VkDescriptorBufferInfo sceneBufferInfo{
            .buffer = rsrcManager.get(sceneParameterBuffer)->buffer,
            .offset = 0,
            .range = sizeof(GPUSceneData),
        };

        VkWriteDescriptorSet camWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = frames[i].globalDescriptor,
            .dstBinding = 0,

            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &camBufferInfo,
        };

        VkWriteDescriptorSet sceneWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = frames[i].globalDescriptor,
            .dstBinding = 1,

            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pBufferInfo = &sceneBufferInfo,
        };

        // Object buffer
        VkDescriptorSetAllocateInfo objectSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,

            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &objectSetLayout,
        };
        vkAllocateDescriptorSets(device, &objectSetAllocInfo, &frames[i].objectDescriptor);

        VkDescriptorBufferInfo objectBufferInfo{
            .buffer = rsrcManager.get(frames[i].objectBuffer)->buffer,
            .offset = 0,
            .range = sizeof(GPUObjectData) * MAX_OBJECTS,
        };
        VkWriteDescriptorSet objectWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = frames[i].objectDescriptor,
            .dstBinding = 0,

            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &objectBufferInfo,
        };

        // write into descriptor sets

        VkWriteDescriptorSet setWrites[] = {camWrite, sceneWrite, objectWrite};

        vkUpdateDescriptorSets(device, 3, setWrites, 0, nullptr);
    }

    deleteQueue.pushBack(
        [=]()
        {
            vkDestroyDescriptorSetLayout(device, globalSetLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, objectSetLayout, nullptr);
        });
}

void VulkanRenderer::initPipelines()
{
    auto& rsrcMngr = *Engine::get()->getResourceManager();

    auto defaultPipeline = rsrcMngr.createGraphicsPipeline({
        .vertexShader = {.sourcePath = SHADERS_PATH "/tri_mesh.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/default_lit.frag"},
    });

    auto texturedPipeline = rsrcMngr.createGraphicsPipeline({
        .vertexShader = {.sourcePath = SHADERS_PATH "/tri_mesh.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/textured_lit.frag"},
    });

    rsrcMngr.createMaterial({.pipeline = defaultPipeline}, "defaultMesh");

    rsrcMngr.createMaterial({.pipeline = texturedPipeline}, "texturedMesh");

    // ALSO BIND TEXTURES TO LAYOUTS ALREADY

    auto lostEmpire = Engine::get()->getResourceManager()->createTexture(
        ASSETS_PATH "/vkguide/lost_empire-RGBA.png", VK_IMAGE_USAGE_SAMPLED_BIT, "empire_diffuse");

    // Descriptors using the loaded textures

    ResourceManager& rsrcManager = *Engine::get()->getResourceManager();

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,

        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    };
    VkSampler blockySampler;
    vkCreateSampler(device, &samplerInfo, nullptr, &blockySampler);

    Material* texturedMat = rsrcManager.get(rsrcManager.getMaterial("texturedMesh"));

    VkDescriptorSetAllocateInfo descSetAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &singleTextureSetLayout,
    };
    vkAllocateDescriptorSets(device, &descSetAllocInfo, &texturedMat->textureSet);

    Handle<Texture> empireDiffuse = rsrcManager.getTexture("empire_diffuse");
    VkDescriptorImageInfo imageBufferInfo{
        .sampler = blockySampler,
        .imageView = rsrcManager.get(empireDiffuse)->imageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet texture1Write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = texturedMat->textureSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageBufferInfo,
    };
    vkUpdateDescriptorSets(device, 1, &texture1Write, 0, nullptr);
}

void VulkanRenderer::cleanup()
{
    if(!isInitialized)
        return;

    deleteQueue.flushReverse();

    vmaDestroyAllocator(allocator);

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    if(enableValidationLayers)
    {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(Engine::get()->getMainWindow()->glfwWindow);
    glfwTerminate();
}

void VulkanRenderer::waitForWorkFinished()
{
    vkDeviceWaitIdle(device);
}

void VulkanRenderer::draw()
{
    const auto& curFrameData = getCurrentFrameData();
    assertVkResult(vkWaitForFences(device, 1, &curFrameData.renderFence, true, UINT64_MAX));
    assertVkResult(vkResetFences(device, 1, &curFrameData.renderFence));

    uint32_t swapchainImageIndex;
    assertVkResult(vkAcquireNextImageKHR(
        device, swapchain, UINT64_MAX, curFrameData.imageAvailableSemaphore, nullptr, &swapchainImageIndex));

    assertVkResult(vkResetCommandBuffer(curFrameData.mainCommandBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,

        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    assertVkResult(vkBeginCommandBuffer(curFrameData.mainCommandBuffer, &cmdBeginInfo));

    auto& rsrcManager = *Engine::get()->getResourceManager();
    Texture* depthTexture = rsrcManager.get(this->depthTexture);
    assert(depthTexture);

    insertImageMemoryBarriers(
        curFrameData.mainCommandBuffer,
        {
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = graphicsQueueFamily,
                .dstQueueFamilyIndex = graphicsQueueFamily,
                .image = swapchainImages[swapchainImageIndex],
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            },
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask =
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask =
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstAccessMask =
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = graphicsQueueFamily,
                .dstQueueFamilyIndex = graphicsQueueFamily,
                .image = depthTexture->image,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            },
        });

    float flash = abs(sin(frameNumber / 120.0f));
    VkClearValue clearValue{.color = {0.0f, 0.0f, flash, 1.0f}};
    VkClearValue depthClear{.depthStencil = {.depth = 1.0f}};

    VkClearValue clearValues[2] = {clearValue, depthClear};

    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = swapchainImageViews[swapchainImageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearValue,
    };

    VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = depthTexture->imageView,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = depthClear,
    };

    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .renderArea =
            {
                .offset = {.x = 0, .y = 0},
                .extent = swapchainExtent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo,
    };

    vkCmdBeginRendering(curFrameData.mainCommandBuffer, &renderingInfo);

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)swapchainExtent.width,
        .height = (float)swapchainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(curFrameData.mainCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{.offset = {0, 0}, .extent = swapchainExtent};
    vkCmdSetScissor(curFrameData.mainCommandBuffer, 0, 1, &scissor);

    // todo: sort before
    drawObjects(curFrameData.mainCommandBuffer, renderables.data(), renderables.size());

    vkCmdEndRendering(curFrameData.mainCommandBuffer);

    // UI Rendering

    VkRenderingAttachmentInfo uiColorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = swapchainImageViews[swapchainImageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfo uiRenderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .renderArea =
            {
                .offset = {.x = 0, .y = 0},
                .extent = swapchainExtent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &uiColorAttachmentInfo,
    };

    vkCmdBeginRendering(curFrameData.mainCommandBuffer, &uiRenderingInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), curFrameData.mainCommandBuffer);

    vkCmdEndRendering(curFrameData.mainCommandBuffer);

    insertImageMemoryBarriers(
        curFrameData.mainCommandBuffer,
        {
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .dstAccessMask = 0,
                .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .image = swapchainImages[swapchainImageIndex],
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            },
        });

    assertVkResult(vkEndCommandBuffer(curFrameData.mainCommandBuffer));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curFrameData.imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,

        .commandBufferCount = 1,
        .pCommandBuffers = &curFrameData.mainCommandBuffer,

        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &curFrameData.renderFinishedSemaphore,
    };

    assertVkResult(vkQueueSubmit(graphicsQueue, 1, &submitInfo, curFrameData.renderFence));

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curFrameData.renderFinishedSemaphore,

        .swapchainCount = 1,
        .pSwapchains = &swapchain,

        .pImageIndices = &swapchainImageIndex,
    };

    assertVkResult(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    frameNumber++;
}

// TODO: refactor to take span?
void VulkanRenderer::drawObjects(VkCommandBuffer cmd, RenderObject* first, int count)
{
    auto& rsrcManager = *Engine::get()->getResourceManager();

    float framed = (frameNumber / 120.0f);
    sceneParameters.ambientColor = {sin(framed), 0, cos(framed), 1};
    char* sceneData = reinterpret_cast<char*>(
        rsrcManager.get(sceneParameterBuffer)->allocInfo.pMappedData); // char so can increment in single bytes
    // dont need to map, was requested to be persistently mapped
    int frameIndex = frameNumber % FRAMES_IN_FLIGHT;
    sceneData += padUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;
    memcpy(sceneData, &sceneParameters, sizeof(GPUSceneData));
    // dont need to unmap, was requested to be coherent

    Camera* mainCamera = Engine::get()->getCamera();
    // todo: resize GPUCameraData to use all matrices Camera stores and then simply memcpy from
    // mainCamera.getMatrices directly
    GPUCameraData camData;
    camData.proj = mainCamera->getProj();
    camData.view = mainCamera->getView();
    camData.projView = mainCamera->getProjView();

    void* data = rsrcManager.get(getCurrentFrameData().cameraBuffer)->allocInfo.pMappedData;
    memcpy(data, &camData, sizeof(GPUCameraData));

    void* objectData = rsrcManager.get(getCurrentFrameData().objectBuffer)->allocInfo.pMappedData;
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    for(int i = 0; i < count; i++)
    {
        const RenderObject& object = first[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }

    Mesh* lastMesh;
    Material* lastMaterial;

    for(int i = 0; i < count; i++)
    {
        RenderObject& object = first[i];
        Mesh* objectMesh = rsrcManager.get(object.mesh);
        Material* objectMaterial = rsrcManager.get(object.material);
        GraphicsPipeline* objectPipeline = rsrcManager.get(objectMaterial->pipeline);

        if(objectMaterial != lastMaterial)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, objectPipeline->pipeline);
            lastMaterial = objectMaterial;

            uint32_t uniformOffset = padUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;
            // Also bind necessary descriptor sets
            // todo: global descriptor set shouldnt need to be bound for every material!
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                objectPipeline->layout,
                0,
                1,
                &getCurrentFrameData().globalDescriptor,
                1,
                &uniformOffset);

            // object data descriptor
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                objectPipeline->layout,
                1,
                1,
                &getCurrentFrameData().objectDescriptor,
                0,
                nullptr);

            if(objectMaterial->textureSet != VK_NULL_HANDLE)
            {
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    objectPipeline->layout,
                    2,
                    1,
                    &objectMaterial->textureSet,
                    0,
                    nullptr);
            }
        }

        MeshPushConstants constants;
        constants.transformMatrix = object.transformMatrix;

        vkCmdPushConstants(
            cmd, objectPipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

        if(objectMesh != lastMesh)
        {
            Buffer* vertexBuffer = rsrcManager.get(objectMesh->vertexBuffer);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer->buffer, &offset);
            lastMesh = objectMesh;
        }

        vkCmdDraw(cmd, objectMesh->vertexCount, 1, 0, i);
    }
}

size_t VulkanRenderer::padUniformBufferSize(size_t originalSize)
{
    size_t minUBOAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if(minUBOAlignment > 0)
    {
        // https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
        alignedSize = (alignedSize + minUBOAlignment - 1) & ~(minUBOAlignment - 1);
    }
    return alignedSize;
}

void VulkanRenderer::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VkCommandBuffer& cmd = uploadContext.commandBuffer;

    // Buffer will be used only once before its reset
    VkCommandBufferBeginInfo cmdBuffBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,

        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    assertVkResult(vkBeginCommandBuffer(cmd, &cmdBuffBeginInfo));
    function(cmd);
    assertVkResult(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,

        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };

    assertVkResult(vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadContext.uploadFence));

    vkWaitForFences(device, 1, &uploadContext.uploadFence, VK_TRUE, 9999999999);
    vkResetFences(device, 1, &uploadContext.uploadFence);

    vkResetCommandPool(device, uploadContext.commandPool, 0);
}