#include "VulkanRenderer.hpp"
#include "../Barriers/Barrier.hpp"
#include "../Mesh/Mesh.hpp"
#include "../RenderObject/RenderObject.hpp"
#include "../Texture/Texture.hpp"
#include "../VulkanTypes.hpp"
#include "Graphics/Renderer/VulkanDebug.hpp"
#include "Graphics/Renderer/VulkanRenderer.hpp"
#include "Init/VulkanDeviceFinder.hpp"
#include "Init/VulkanInit.hpp"
#include "Init/VulkanSwapchainSetup.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "VulkanDebug.hpp"
#include <Engine/Scene/DefaultComponents.hpp>

#include <Datastructures/Span.hpp>
#include <Engine.hpp>
#include <cstddef>

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
#include <unordered_map>
#include <vulkan/vulkan_core.h>

void VulkanRenderer::init()
{
    ptr = this;

    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();
    initBindless();
    initPipelineCache();

    initImGui();
    initGlobalBuffers();

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
    depthTexture = rsrcManager.createTexture(Texture::Info{
        .debugName = "Depth Texture",
        .size = {swapchainExtent.width, swapchainExtent.height, 1},
        .format = depthFormat,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .viewAspect = VK_IMAGE_ASPECT_DEPTH_BIT,
    });

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

        .flags = 0,
        .queueFamilyIndex = graphicsQueueFamily,
    };

    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        assertVkResult(vkCreateCommandPool(device, &commandPoolCrInfo, nullptr, &perFrameData[i].commandPool));

        VkCommandBufferAllocateInfo cmdBuffAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,

            .commandPool = perFrameData[i].commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        assertVkResult(vkAllocateCommandBuffers(device, &cmdBuffAllocInfo, &perFrameData[i].mainCommandBuffer));

        deleteQueue.pushBack([=]() { vkDestroyCommandPool(device, perFrameData[i].commandPool, nullptr); });
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
        assertVkResult(vkCreateFence(device, &fenceCreateInfo, nullptr, &perFrameData[i].renderFence));

        deleteQueue.pushBack([=]() { vkDestroyFence(device, perFrameData[i].renderFence, nullptr); });

        assertVkResult(
            vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &perFrameData[i].imageAvailableSemaphore));
        assertVkResult(
            vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &perFrameData[i].renderFinishedSemaphore));

        deleteQueue.pushBack(
            [=]()
            {
                vkDestroySemaphore(device, perFrameData[i].imageAvailableSemaphore, nullptr);
                vkDestroySemaphore(device, perFrameData[i].renderFinishedSemaphore, nullptr);
            });
    }

    VkFenceCreateInfo uploadFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
    };
    assertVkResult(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadContext.uploadFence));
    deleteQueue.pushBack([=]() { vkDestroyFence(device, uploadContext.uploadFence, nullptr); });
}

void VulkanRenderer::initBindless()
{
    bindlessManager.init();

    VkPushConstantRange basicPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = sizeof(BindlessIndices),
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = (uint32_t)bindlessManager.bindlessSetLayouts.size(),
        .pSetLayouts = bindlessManager.bindlessSetLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &basicPushConstantRange,
    };
    vkCreatePipelineLayout(device, &pipelineLayoutCrInfo, nullptr, &bindlessPipelineLayout);

    deleteQueue.pushBack([=]() { vkDestroyPipelineLayout(device, bindlessPipelineLayout, nullptr); });
}

void VulkanRenderer::initPipelineCache()
{
    VkPipelineCacheCreateInfo cacheCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    // VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT ?

    std::vector<char> buffer;
    bool cacheFileExists = false;
    {
        std::ifstream file(pipelineCacheFile.data(), std::ios::ate | std::ios::binary);
        if(file.is_open())
        {
            auto fileSize = (size_t)file.tellg();
            buffer.resize(fileSize);
            file.seekg(0);
            file.read(buffer.data(), fileSize);
            file.close();

            // Check if cache can be used by seeing if header matches
            auto* cacheHeader = (VkPipelineCacheHeaderVersionOne*)buffer.data();
            if(cacheHeader->deviceID == physicalDeviceProperties.deviceID &&
               cacheHeader->vendorID == physicalDeviceProperties.vendorID &&
               (memcmp(cacheHeader->pipelineCacheUUID, physicalDeviceProperties.pipelineCacheUUID, VK_UUID_SIZE) ==
                0))
            {
                cacheFileExists = true;
            }
        }
    }

    if(cacheFileExists)
    {
        cacheCrInfo.initialDataSize = (uint32_t)buffer.size();
        cacheCrInfo.pInitialData = buffer.data();
    }
    else
    {
        cacheCrInfo.initialDataSize = 0;
        cacheCrInfo.pInitialData = nullptr;
    }

    vkCreatePipelineCache(device, &cacheCrInfo, nullptr, &pipelineCache);

    deleteQueue.pushBack([=]() { vkDestroyPipelineCache(device, pipelineCache, nullptr); });
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
        .PipelineCache = pipelineCache,
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

void VulkanRenderer::initGlobalBuffers()
{
    ResourceManager* rm = ResourceManager::get();
    assert(rm);

    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        perFrameData[i].cameraBuffer = rm->createBuffer(Buffer::CreateInfo{
            .info =
                {
                    .size = sizeof(RenderPassData),
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
        perFrameData[i].objectBuffer = rm->createBuffer(Buffer::CreateInfo{
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
    }
}

void VulkanRenderer::savePipelineCache()
{
    size_t cacheDataSize = 0;
    vkGetPipelineCacheData(device, pipelineCache, &cacheDataSize, nullptr);
    std::vector<char> cacheData;
    cacheData.resize(cacheDataSize);
    vkGetPipelineCacheData(device, pipelineCache, &cacheDataSize, cacheData.data());

    std::ofstream outFile(pipelineCacheFile.data(), std::ios::out | std::ios::binary);
    outFile.write(cacheData.data(), cacheData.size());
    outFile.close();
}

void VulkanRenderer::cleanup()
{
    if(!isInitialized)
        return;

    savePipelineCache();

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

    // Reset all command buffers for current frame
    assertVkResult(vkResetCommandPool(device, curFrameData.commandPool, 0));

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,

        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    assertVkResult(vkBeginCommandBuffer(curFrameData.mainCommandBuffer, &cmdBeginInfo));

    // Bind the bindless descriptor sets once per cmdbuffer
    vkCmdBindDescriptorSets(
        curFrameData.mainCommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        bindlessPipelineLayout,
        0,
        4,
        bindlessManager.bindlessDescriptorSets.data(),
        0,
        nullptr);

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

    float flash = abs(sin(frameNumber / 1200.0f));
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

    renderables.clear();
    Engine::get()->ecs.forEach<RenderInfo, Transform>(
        [&](RenderInfo* renderinfos, Transform* transforms, uint32_t count)
        {
            for(int i = 0; i < count; i++)
            {
                const RenderInfo& rinfo = renderinfos[i];
                const Transform& transform = transforms[i];
                renderables.emplace_back(rinfo.mesh, rinfo.materialInstance, transform.localToWorld);
            }
        });
    // todo: sort before passing to drawObjects
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

// TODO: take span
void VulkanRenderer::drawObjects(VkCommandBuffer cmd, RenderObject* first, int count)
{
    ResourceManager* rm = ResourceManager::get();

    Camera* mainCamera = Engine::get()->getCamera();

    RenderPassData renderPassData;
    renderPassData.proj = mainCamera->getProj();
    renderPassData.view = mainCamera->getView();
    renderPassData.projView = mainCamera->getProjView();
    renderPassData.cameraPositionWS = mainCamera->getPosition();

    Buffer* cameraBuffer = rm->get(getCurrentFrameData().cameraBuffer);

    void* data = cameraBuffer->allocInfo.pMappedData;
    memcpy(data, &renderPassData, sizeof(renderPassData));

    Buffer* transformBuffer = rm->get(getCurrentFrameData().objectBuffer);

    void* objectData = transformBuffer->allocInfo.pMappedData;
    // not sure how good assigning single GPUObjectDatas is (vs CPU buffer and then one memcpy)
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    for(int i = 0; i < count; i++)
    {
        const RenderObject& object = first[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }

    //---

    BindlessIndices pushConstants;
    pushConstants.RenderInfoBuffer = cameraBuffer->uniformResourceIndex;
    pushConstants.transformBuffer = transformBuffer->storageResourceIndex;

    Handle<Mesh> lastMesh = Handle<Mesh>::Invalid();
    Handle<Material> lastMaterial = Handle<Material>::Invalid();
    Handle<MaterialInstance> lastMaterialInstance = Handle<MaterialInstance>::Invalid();
    uint32_t indexCount = 0;

    for(int i = 0; i < count; i++)
    {
        RenderObject& object = first[i];

        Handle<Mesh> objectMesh = object.mesh;
        Handle<MaterialInstance> objectMaterialInstance = object.materialInstance;

        if(objectMaterialInstance != lastMaterialInstance)
        {
            MaterialInstance* newMatInst = rm->get(objectMaterialInstance);
            if(newMatInst->parentMaterial != lastMaterial)
            {
                Material* newMat = rm->get(newMatInst->parentMaterial);
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newMat->pipeline);
                Buffer* materialParamsBuffer = rm->get(newMat->parameters.getGPUBuffer());
                if(materialParamsBuffer != nullptr)
                {
                    pushConstants.materialParamsBuffer = materialParamsBuffer->uniformResourceIndex;
                }
                lastMaterial = newMatInst->parentMaterial;
            }

            Buffer* materialInstanceParamsBuffer = rm->get(newMatInst->parameters.getGPUBuffer());
            if(materialInstanceParamsBuffer != nullptr)
            {
                pushConstants.materialInstanceParamsBuffer = materialInstanceParamsBuffer->uniformResourceIndex;
            }
        }

        pushConstants.transformIndex = i;
        vkCmdPushConstants(
            cmd, bindlessPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(BindlessIndices), &pushConstants);

        if(objectMesh != lastMesh)
        {
            Mesh* newMesh = rm->get(objectMesh);
            indexCount = newMesh->indexCount;
            Buffer* indexBuffer = rm->get(newMesh->indexBuffer);
            Buffer* positionBuffer = rm->get(newMesh->positionBuffer);
            Buffer* attributeBuffer = rm->get(newMesh->attributeBuffer);
            const VkBuffer buffers[2] = {positionBuffer->buffer, attributeBuffer->buffer};
            const VkDeviceSize offsets[2] = {0, 0};
            vkCmdBindIndexBuffer(cmd, indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindVertexBuffers(cmd, 0, 2, &buffers[0], &offsets[0]);
            lastMesh = objectMesh;
        }

        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
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