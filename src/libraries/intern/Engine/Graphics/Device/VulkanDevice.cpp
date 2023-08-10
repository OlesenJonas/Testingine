#include "VulkanDevice.hpp"
#include "VulkanDebug.hpp"

#include "Graphics/Barrier/Barrier.hpp"
#include "Init/VulkanDeviceFinder.hpp"
#include "Init/VulkanInit.hpp"
#include "Init/VulkanSwapchainSetup.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include <Engine/Graphics/Device/VulkanConversions.hpp>

#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <fstream>
#include <stdexcept>
#include <thread>

PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT;

void VulkanDevice::init(GLFWwindow* window)
{
    INIT_STATIC_GETTER();
    mainWindow = window;

    initVulkan();
    initPipelineCache();

    // everything went fine

    // per frame stuff
    initSwapchain();
    initCommands();
    initSyncStructures();

    // TODO: where to put this?
    initBindless();

    initImGui();

    _initialized = true;
}

uint32_t VulkanDevice::getSwapchainWidth()
{
    return swapchainExtent.width;
}
uint32_t VulkanDevice::getSwapchainHeight()
{
    return swapchainExtent.height;
}

void VulkanDevice::initVulkan()
{
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
    const std::vector<const char*> instanceExtensions = {
        // Need this for debug markers, not just for layers
        // TODO: check if extensions are even support, save through some flag etc
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    instance = createInstance(enableValidationLayers, validationLayers, instanceExtensions);

    if(enableValidationLayers)
        debugMessenger = setupDebugMessenger(instance, &breakOnValidationError);

    if(glfwCreateWindowSurface(instance, mainWindow, nullptr, &surface) != VK_SUCCESS)
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

    // get device related function pointers
    pfnCmdBeginDebugUtilsLabelEXT =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
    pfnCmdEndDebugUtilsLabelEXT =
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");

    queueFamilyIndices = deviceFinder.getQueueFamilyIndices();
    graphicsAndComputeQueueFamily = queueFamilyIndices.graphicsAndComputeFamily.value();

    vkGetDeviceQueue(device, graphicsAndComputeQueueFamily, 0, &graphicsAndComputeQueue);
    // vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    VmaAllocatorCreateInfo vmaAllocatorCrInfo{
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
    };
    vmaCreateAllocator(&vmaAllocatorCrInfo, &allocator);
}

void VulkanDevice::initPipelineCache()
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

void VulkanDevice::initSwapchain()
{
    VulkanSwapchainSetup swapchainSetup(physicalDevice, device, mainWindow, surface);
    swapchainSetup.setup(queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value());

    swapchain = swapchainSetup.getSwapchain();
    swapchainExtent = swapchainSetup.getSwapchainExtent();
    swapchainImageFormat = swapchainSetup.getSwapchainImageFormat();
    swapchainImages = swapchainSetup.getSwapchainImages();

    swapchainImageViews = swapchainSetup.createSwapchainImageViews();

    deleteQueue.pushBack([=]() { vkDestroySwapchainKHR(device, swapchain, nullptr); });
    for(VkImageView view : swapchainImageViews)
    {
        deleteQueue.pushBack([=]() { vkDestroyImageView(device, view, nullptr); });
    }
}

void VulkanDevice::initCommands()
{
    VkCommandPoolCreateInfo commandPoolCrInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,

        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = graphicsAndComputeQueueFamily,
    };

    uint32_t threadCount = std::thread::hardware_concurrency();
    assert(threadCount > 0);
    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        PerFrameData& frameData = perFrameData[i];
        frameData.commandPools.resize(threadCount);
        frameData.usedCommandBuffersPerPool.resize(threadCount);
        for(int t = 0; t < threadCount; t++)
        {
            assertVkResult(
                vkCreateCommandPool(device, &commandPoolCrInfo, nullptr, &perFrameData[i].commandPools[t]));

            deleteQueue.pushBack([=]()
                                 { vkDestroyCommandPool(device, perFrameData[i].commandPools[t], nullptr); });
        }
    }

    VkCommandPoolCreateInfo uploadCommandPoolCrInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .queueFamilyIndex = graphicsAndComputeQueueFamily,
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

void VulkanDevice::initSyncStructures()
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
        assertVkResult(vkCreateFence(device, &fenceCreateInfo, nullptr, &perFrameData[i].commandsDone));

        deleteQueue.pushBack([=]() { vkDestroyFence(device, perFrameData[i].commandsDone, nullptr); });

        assertVkResult(
            vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &perFrameData[i].swapchainImageAvailable));
        assertVkResult(vkCreateSemaphore(
            device, &semaphoreCreateInfo, nullptr, &perFrameData[i].swapchainImageRenderFinished));

        deleteQueue.pushBack(
            [=]()
            {
                vkDestroySemaphore(device, perFrameData[i].swapchainImageRenderFinished, nullptr);
                vkDestroySemaphore(device, perFrameData[i].swapchainImageAvailable, nullptr);
            });
    }

    VkFenceCreateInfo uploadFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
    };
    assertVkResult(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadContext.uploadFence));
    deleteQueue.pushBack([=]() { vkDestroyFence(device, uploadContext.uploadFence, nullptr); });
}

void VulkanDevice::initBindless()
{
    bindlessManager.init();

    VkPushConstantRange basicPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = BindlessManager::maxBindlessPushConstantSize,
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = bindlessManager.getDescriptorSetsCount(),
        .pSetLayouts = bindlessManager.getDescriptorSetLayouts(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &basicPushConstantRange,
    };
    vkCreatePipelineLayout(device, &pipelineLayoutCrInfo, nullptr, &bindlessPipelineLayout);

    deleteQueue.pushBack([=]() { vkDestroyPipelineLayout(device, bindlessPipelineLayout, nullptr); });
}

void VulkanDevice::initImGui()
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
    ImGui_ImplGlfw_InitForVulkan(mainWindow, true);
    // init imgui for Vulkan
    ImGui_ImplVulkan_InitInfo initInfo = {
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,
        .QueueFamily = graphicsAndComputeQueueFamily,
        .Queue = graphicsAndComputeQueue,
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

void VulkanDevice::savePipelineCache()
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

void VulkanDevice::cleanup()
{
    if(!_initialized)
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

    // TODO: why does vulkan renderer call this, should be responsibility of class owning main window
    glfwDestroyWindow(mainWindow);
    glfwTerminate();
}

void VulkanDevice::waitForWorkFinished()
{
    vkDeviceWaitIdle(device);
}

void VulkanDevice::startNextFrame()
{
    frameNumber++;

    auto& curFrameData = getCurrentFrameData();

    assertVkResult(vkWaitForFences(device, 1, &curFrameData.commandsDone, true, UINT64_MAX));
    assertVkResult(vkResetFences(device, 1, &curFrameData.commandsDone));

    assertVkResult(vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX,
        curFrameData.swapchainImageAvailable,
        nullptr,
        &currentSwapchainImageIndex));

    // TODO: not sure if I really want to free them all, could keep them around
    //       would then also not have to allocate new ones every frame!
    for(int i = 0; i < curFrameData.usedCommandBuffersPerPool.size(); i++)
    {
        if(!curFrameData.usedCommandBuffersPerPool[i].empty())
        {
            // Free command buffers that were used
            vkFreeCommandBuffers(
                device,
                curFrameData.commandPools[i],
                curFrameData.usedCommandBuffersPerPool[i].size(),
                curFrameData.usedCommandBuffersPerPool[i].data());
            curFrameData.usedCommandBuffersPerPool[i].clear();
        }
    }
    for(VkCommandPool cmdPool : curFrameData.commandPools)
    {
        assertVkResult(vkResetCommandPool(device, cmdPool, 0));
    }
}

VkCommandBuffer VulkanDevice::beginCommandBuffer(uint32_t threadIndex)
{
    auto& curFrameData = getCurrentFrameData();

    VkCommandBuffer cmdBuffer;
    VkCommandBufferAllocateInfo cmdBuffAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = curFrameData.commandPools[threadIndex],
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(device, &cmdBuffAllocInfo, &cmdBuffer);
    curFrameData.usedCommandBuffersPerPool[threadIndex].push_back(cmdBuffer);

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,

        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    assertVkResult(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo));

    // Bind the bindless descriptor sets once per cmdbuffer
    // TODO: overkill always binding both, parameterize
    vkCmdBindDescriptorSets(
        cmdBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        bindlessPipelineLayout,
        0,
        bindlessManager.getDescriptorSetsCount(),
        bindlessManager.getDescriptorSets(),
        0,
        nullptr);
    vkCmdBindDescriptorSets(
        cmdBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        bindlessPipelineLayout,
        0,
        bindlessManager.getDescriptorSetsCount(),
        bindlessManager.getDescriptorSets(),
        0,
        nullptr);

    return cmdBuffer;
}
void VulkanDevice::endCommandBuffer(VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);
}

void VulkanDevice::submitCommandBuffers(Span<const VkCommandBuffer> cmdBuffers)
{
    const auto& curFrameData = getCurrentFrameData();

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curFrameData.swapchainImageAvailable,
        .pWaitDstStageMask = &waitStage,

        .commandBufferCount = static_cast<uint32_t>(cmdBuffers.size()),
        .pCommandBuffers = cmdBuffers.data(),

        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &curFrameData.swapchainImageRenderFinished,
    };

    assertVkResult(vkQueueSubmit(graphicsAndComputeQueue, 1, &submitInfo, curFrameData.commandsDone));
}

void VulkanDevice::insertSwapchainImageBarrier(
    VkCommandBuffer cmd, ResourceState currentState, ResourceState targetState)
{
    VkImageMemoryBarrier2 imageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = toVkPipelineStage(currentState),
        .srcAccessMask = toVkAccessFlags(currentState),
        .dstStageMask = toVkPipelineStage(targetState),
        .dstAccessMask = toVkAccessFlags(targetState),
        .oldLayout = toVkImageLayout(currentState),
        .newLayout = toVkImageLayout(targetState),
        .srcQueueFamilyIndex = graphicsAndComputeQueueFamily,
        .dstQueueFamilyIndex = graphicsAndComputeQueueFamily,
        .image = swapchainImages[currentSwapchainImageIndex],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    const VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

// TODO: overload to just not take a depth target, instead of having to pass null inside render target?
void VulkanDevice::beginRendering(
    VkCommandBuffer cmd, Span<const RenderTarget>&& colorTargets, RenderTarget&& depthTarget)
{
    ResourceManager* rm = ResourceManager::get();

    VkClearValue clearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}};
    VkClearValue depthStencilClear{.depthStencil = {.depth = 1.0f, .stencil = 0u}};

    std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos;
    colorAttachmentInfos.reserve(colorTargets.size());
    for(const auto& target : colorTargets)
    {
        const VkImageView view = std::holds_alternative<Handle<Texture>>(target.texture)
                                     ? rm->get(std::get<Handle<Texture>>(depthTarget.texture))->fullResourceView()
                                     : swapchainImageViews[currentSwapchainImageIndex];
        colorAttachmentInfos.emplace_back(VkRenderingAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = view,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = toVkLoadOp(target.loadOp),
            .storeOp = toVkStoreOp(target.storeOp),
            .clearValue = clearValue,
        });
    }

    VkRenderingAttachmentInfo depthAttachmentInfo;

    bool hasDepthAttachment = std::get<Handle<Texture>>(depthTarget.texture).isValid();
    if(hasDepthAttachment)
        depthAttachmentInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = rm->get(std::get<Handle<Texture>>(depthTarget.texture))->fullResourceView(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .loadOp = toVkLoadOp(depthTarget.loadOp),
            .storeOp = toVkStoreOp(depthTarget.storeOp),
            .clearValue = depthStencilClear,
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
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentInfos.size()),
        .pColorAttachments = colorAttachmentInfos.data(),
        .pDepthAttachment = hasDepthAttachment ? &depthAttachmentInfo : nullptr,
    };

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)swapchainExtent.width,
        .height = (float)swapchainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{.offset = {0, 0}, .extent = swapchainExtent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanDevice::endRendering(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
}

void VulkanDevice::insertBarriers(VkCommandBuffer cmd, Span<const Barrier> barriers)
{
    std::vector<VkImageMemoryBarrier2> imageBarriers;
    std::vector<VkBufferMemoryBarrier2> bufferBarriers;

    for(const auto& barrier : barriers)
    {
        if(barrier.type == Barrier::Type::Buffer)
        {
            assert(false);
        }
        else if(barrier.type == Barrier::Type::Image)
        {
            auto* rm = ResourceManager::get();

            const Texture* tex = rm->get(barrier.image.texture);
            if(tex == nullptr)
            {
                BREAKPOINT;
                continue; // TODO: LOG warning
            }
            assert(tex->descriptor.mipLevels > 0);
            int32_t mipCount = barrier.image.mipCount == Texture::MipLevels::All
                                   ? tex->descriptor.mipLevels - barrier.image.mipLevel
                                   : barrier.image.mipCount;

            VkImageMemoryBarrier2 vkBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,

                .srcStageMask = toVkPipelineStage(barrier.image.stateBefore),
                .srcAccessMask = toVkAccessFlags(barrier.image.stateBefore),

                .dstStageMask = toVkPipelineStage(barrier.image.stateAfter),
                .dstAccessMask = toVkAccessFlags(barrier.image.stateAfter),

                .oldLayout = barrier.image.allowDiscardOriginal ? VK_IMAGE_LAYOUT_UNDEFINED
                                                                : toVkImageLayout(barrier.image.stateBefore),
                .newLayout = toVkImageLayout(barrier.image.stateAfter),

                .srcQueueFamilyIndex = graphicsAndComputeQueueFamily,
                .dstQueueFamilyIndex = graphicsAndComputeQueueFamily,

                .image = tex->image,
                .subresourceRange =
                    {
                        .aspectMask = toVkImageAspect(tex->descriptor.format),
                        .baseMipLevel = static_cast<uint32_t>(barrier.image.mipLevel),
                        .levelCount = static_cast<uint32_t>(mipCount),
                        .baseArrayLayer = static_cast<uint32_t>(barrier.image.arrayLayer),
                        .layerCount = static_cast<uint32_t>(
                            tex->descriptor.type == Texture::Type::tCube ? 6 * barrier.image.arrayLength
                                                                         : barrier.image.arrayLength),
                    },
            };

            imageBarriers.emplace_back(vkBarrier);
        }
        else
        {
            assert(false);
        }
    }

    if(imageBarriers.empty() && bufferBarriers.empty())
        return;

    const VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = uint32_t(bufferBarriers.size()),
        .pBufferMemoryBarriers = bufferBarriers.data(),
        .imageMemoryBarrierCount = uint32_t(imageBarriers.size()),
        .pImageMemoryBarriers = imageBarriers.data(),
    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    bufferBarriers.clear();
    imageBarriers.clear();
}

void VulkanDevice::setGraphicsPipelineState(VkCommandBuffer cmd, Handle<Material> mat)
{
    auto* rm = ResourceManager::get();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rm->get(mat)->pipeline);
}

void VulkanDevice::setComputePipelineState(VkCommandBuffer cmd, Handle<ComputeShader> shader)
{
    auto* rm = ResourceManager::get();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rm->get(shader)->pipeline);
}

// TODO: CORRECT PUSH CONSTANT RANGES FOR COMPUTE PIPELINES !!!!!
void VulkanDevice::pushConstants(VkCommandBuffer cmd, size_t size, void* data, size_t offset)
{
    vkCmdPushConstants(cmd, bindlessPipelineLayout, VK_SHADER_STAGE_ALL, offset, size, data);
}

void VulkanDevice::bindIndexBuffer(VkCommandBuffer cmd, Handle<Buffer> buffer, size_t offset)
{
    auto* rm = ResourceManager::get();
    vkCmdBindIndexBuffer(cmd, rm->get(buffer)->buffer, offset, VK_INDEX_TYPE_UINT32);
}

void VulkanDevice::bindVertexBuffers(
    VkCommandBuffer cmd,
    uint32_t startBinding,
    uint32_t count,
    Span<const Handle<Buffer>> buffers,
    Span<const uint64_t> offsets)
{
    auto* rm = ResourceManager::get();

    assert(buffers.size() == offsets.size());
    std::vector<VkBuffer> vkBuffers{buffers.size()};
    for(int i = 0; i < buffers.size(); i++)
    {
        vkBuffers[i] = rm->get(buffers[i])->buffer;
        assert(vkBuffers[i]);
    }
    vkCmdBindVertexBuffers(cmd, startBinding, count, &vkBuffers[0], offsets.data());
}

void VulkanDevice::drawIndexed(
    VkCommandBuffer cmd,
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    uint32_t vertexOffset,
    uint32_t firstInstance)
{
    vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VulkanDevice::drawImGui(VkCommandBuffer cmd)
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void VulkanDevice::dispatchCompute(VkCommandBuffer cmd, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);
}

void VulkanDevice::presentSwapchain()
{
    const auto& curFrameData = getCurrentFrameData();

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curFrameData.swapchainImageRenderFinished,

        .swapchainCount = 1,
        .pSwapchains = &swapchain,

        .pImageIndices = &currentSwapchainImageIndex,
    };

    assertVkResult(vkQueuePresentKHR(graphicsAndComputeQueue, &presentInfo));
}

void VulkanDevice::disableValidationErrorBreakpoint()
{
    breakOnValidationError = false;
}
void VulkanDevice::enableValidationErrorBreakpoint()
{
    breakOnValidationError = true;
}

void VulkanDevice::startDebugRegion(VkCommandBuffer cmd, const char* name)
{
    VkDebugUtilsLabelEXT info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = name,
    };
    pfnCmdBeginDebugUtilsLabelEXT(cmd, &info);
}
void VulkanDevice::endDebugRegion(VkCommandBuffer cmd)
{
    pfnCmdEndDebugUtilsLabelEXT(cmd);
}

void VulkanDevice::setDebugName(VkObjectType type, uint64_t handle, const char* name)
{
    if(setObjectDebugName == nullptr)
    {
        setObjectDebugName =
            (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
        assert(setObjectDebugName);
    }
    VkDebugUtilsObjectNameInfoEXT nameInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = type,
        .objectHandle = handle,
        .pObjectName = name,
    };
    setObjectDebugName(device, &nameInfo);
}

void VulkanDevice::fillMipLevels(VkCommandBuffer cmd, Handle<Texture> texture, ResourceState state)
{
    /*
        TODO: check that image was created with usage_transfer_src_bit !
              Switch to compute shader based solution? Could get rid of needing to mark
              *all* textures as transfer_src/dst, just because mips are needed.
              But requires a lot more work to handle different texture types (3d, cube, array)
    */

    auto* rm = ResourceManager::get();

    Texture* tex = rm->get(texture);

    if(tex->descriptor.mipLevels == 1)
        return;

    uint32_t startWidth = tex->descriptor.size.width;
    uint32_t startHeight = tex->descriptor.size.height;
    uint32_t startDepth = tex->descriptor.size.depth;

    // Transition mip 0 to transfer source
    if(state != ResourceState::TransferSrc)
    {
        insertBarriers(
            cmd,
            {
                Barrier::from(Barrier::Image{
                    .texture = texture,
                    .stateBefore = state,
                    .stateAfter = ResourceState::TransferSrc,
                    .mipLevel = 0,
                    .mipCount = 1,
                    // TODO: also have a Texture::ArrayLayers::All value? And set that as default?
                    .arrayLength = int32_t(tex->descriptor.arrayLength),
                }),
            });
    }

    // Generate mip chain
    // Downscaling from each level successively, but could also downscale mip 0 -> mip N each time

    for(int i = 1; i < tex->descriptor.mipLevels; i++)
    {
        // prepare current level to be transfer dst
        if(state != ResourceState::TransferSrc)
        {
            insertBarriers(
                cmd,
                {
                    Barrier::from(Barrier::Image{
                        .texture = texture,
                        .stateBefore = state,
                        .stateAfter = ResourceState::TransferDst,
                        .mipLevel = i,
                        .mipCount = 1,
                        .arrayLength = int32_t(tex->descriptor.arrayLength),
                    }),
                });
        }

        uint32_t lastWidth = std::max(startWidth >> (i - 1), 1u);
        uint32_t lastHeight = std::max(startHeight >> (i - 1), 1u);
        uint32_t lastDepth = std::max(startDepth >> (i - 1), 1u);

        uint32_t curWidth = std::max(startWidth >> i, 1u);
        uint32_t curHeight = std::max(startHeight >> i, 1u);
        uint32_t curDepth = std::max(startDepth >> i, 1u);

        VkImageBlit blit{
            .srcSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = uint32_t(i - 1),
                    .baseArrayLayer = 0,
                    .layerCount = toVkArrayLayers(tex->descriptor),
                },
            .srcOffsets =
                {{.x = 0, .y = 0, .z = 0}, {int32_t(lastWidth), int32_t(lastHeight), int32_t(lastDepth)}},
            .dstSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = uint32_t(i),
                    .baseArrayLayer = 0,
                    .layerCount = toVkArrayLayers(tex->descriptor),
                },
            .dstOffsets = {{.x = 0, .y = 0, .z = 0}, {int32_t(curWidth), int32_t(curHeight), int32_t(curDepth)}},
        };

        // do the blit
        vkCmdBlitImage(
            cmd,
            tex->image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            tex->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR);

        // prepare current level to be transfer src for next mip
        insertBarriers(
            cmd,
            {
                Barrier::from(Barrier::Image{
                    .texture = texture,
                    // TODO: need better way to determine initial state, pass as parameter?
                    .stateBefore = ResourceState::TransferDst,
                    .stateAfter = ResourceState::TransferSrc,
                    .mipLevel = i,
                    .mipCount = 1,
                    .arrayLength = int32_t(tex->descriptor.arrayLength),
                }),
            });
    }

    // after the loop, transfer all mips to input state
    insertBarriers(
        cmd,
        {
            Barrier::from(Barrier::Image{
                .texture = texture,
                .stateBefore = ResourceState::TransferSrc,
                .stateAfter = state,
                .mipLevel = 0,
                .mipCount = tex->descriptor.mipLevels,
                .arrayLength = int32_t(tex->descriptor.arrayLength),
            }),
        });
}

size_t VulkanDevice::padUniformBufferSize(size_t originalSize)
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

void VulkanDevice::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
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

    assertVkResult(vkQueueSubmit(graphicsAndComputeQueue, 1, &submitInfo, uploadContext.uploadFence));

    vkWaitForFences(device, 1, &uploadContext.uploadFence, VK_TRUE, 9999999999);
    vkResetFences(device, 1, &uploadContext.uploadFence);

    vkResetCommandPool(device, uploadContext.commandPool, 0);
}