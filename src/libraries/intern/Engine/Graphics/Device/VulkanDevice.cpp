#include "VulkanDevice.hpp"
#include "../Barrier/Barrier.hpp"
#include "../Mesh/Mesh.hpp"
#include "Init/VulkanDeviceFinder.hpp"
#include "Init/VulkanInit.hpp"
#include "Init/VulkanSwapchainSetup.hpp"
#include "VulkanConversions.hpp"
#include "VulkanDebug.hpp"

#include <Datastructures/ArrayHelpers.hpp>

#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <VMA/VMA.hpp>
#include <fstream>
#include <stdexcept>
#include <thread>

PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT;

void VulkanDevice::init(GLFWwindow* window)
{
    INIT_STATIC_GETTER();
    mainWindow = window;

    bufferPool.init(10);
    texturePool.init(10);
    textureViewPool.init(10);
    samplerPool.init(BindlessManager::samplerLimit);

    initVulkan();
    initPipelineCache();

    // per frame stuff
    initSwapchain();
    initCommands();
    initSyncStructures();
    initAllocators();

    // TODO: where to put this?
    initBindless();

    initImGui();

    _initialized = true;
}

void VulkanDevice::destroyResources()
{
    auto clearPool = [&](auto&& pool)
    {
        for(auto iter = pool.begin(); iter != pool.end(); iter++)
        {
            destroy(iter.asHandle());
        }
    };
    auto clearMultiPool = [&](auto&& pool)
    {
        for(auto iter = pool.begin(); iter != pool.end(); iter++)
        {
            destroy(*iter);
        }
    };
    clearPool(samplerPool);
    clearPool(textureViewPool);
    clearMultiPool(texturePool);
    clearMultiPool(bufferPool);
}

uint32_t VulkanDevice::getSwapchainWidth() { return swapchainExtent.width; }
uint32_t VulkanDevice::getSwapchainHeight() { return swapchainExtent.height; }

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

            deleteQueue.pushBack([=, pool = perFrameData[i].commandPools[t]]()
                                 { vkDestroyCommandPool(device, pool, nullptr); });

            // Per frame upload stuff
            VkCommandPoolCreateInfo uploadCommandPoolCrInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,

                .flags = 0,
                .queueFamilyIndex = graphicsAndComputeQueueFamily,
            };
            assertVkResult(
                vkCreateCommandPool(device, &uploadCommandPoolCrInfo, nullptr, &frameData.uploadCommandPool));
            deleteQueue.pushBack([device = device, pool = frameData.uploadCommandPool]()
                                 { vkDestroyCommandPool(device, pool, nullptr); });

            VkCommandBufferAllocateInfo cmdBuffAllocInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,

                .commandPool = frameData.uploadCommandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            assertVkResult(vkAllocateCommandBuffers(device, &cmdBuffAllocInfo, &frameData.uploadCommandBuffer));
        }
    }

    // Immediate submit
    {
        VkCommandPoolCreateInfo uploadCommandPoolCrInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,

            .flags = 0,
            .queueFamilyIndex = graphicsAndComputeQueueFamily,
        };
        assertVkResult(vkCreateCommandPool(device, &uploadCommandPoolCrInfo, nullptr, &uploadContext.commandPool));
        deleteQueue.pushBack([=, pool = uploadContext.commandPool]()
                             { vkDestroyCommandPool(device, pool, nullptr); });

        VkCommandBufferAllocateInfo cmdBuffAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,

            .commandPool = uploadContext.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        assertVkResult(vkAllocateCommandBuffers(device, &cmdBuffAllocInfo, &uploadContext.commandBuffer));
    }
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

        deleteQueue.pushBack([=, fence = perFrameData[i].commandsDone]()
                             { vkDestroyFence(device, fence, nullptr); });

        assertVkResult(
            vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &perFrameData[i].swapchainImageAvailable));
        assertVkResult(vkCreateSemaphore(
            device, &semaphoreCreateInfo, nullptr, &perFrameData[i].swapchainImageRenderFinished));

        deleteQueue.pushBack(
            [=,
             renderFinished = perFrameData[i].swapchainImageRenderFinished,
             imgAvail = perFrameData[i].swapchainImageAvailable]()
            {
                vkDestroySemaphore(device, renderFinished, nullptr);
                vkDestroySemaphore(device, imgAvail, nullptr);
            });
    }

    VkFenceCreateInfo uploadFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
    };
    assertVkResult(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadContext.uploadFence));
    deleteQueue.pushBack([=]() { vkDestroyFence(device, uploadContext.uploadFence, nullptr); });
}

#define MB(x) ((size_t)(x) << 20)

void VulkanDevice::initAllocators()
{
    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        auto& frameData = perFrameData[i];
        constexpr size_t stagingBufferSize = MB(50);
        frameData.stagingAllocator.buffer = createBuffer(Buffer::CreateInfo{
            .debugName = "StagingBuffer" + std::to_string(i),
            .size = stagingBufferSize,
            .memoryType = Buffer::MemoryType::CPU,
            .allStates = ResourceState::TransferSrc,
            .initialState = ResourceState::TransferSrc,
        });
        frameData.stagingAllocator.capacity = stagingBufferSize;
        frameData.stagingAllocator.ptr = static_cast<uint8_t*>(*get<void*>(frameData.stagingAllocator.buffer));
    }
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

    destroyResources();

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

void VulkanDevice::waitForWorkFinished() { vkDeviceWaitIdle(device); }

Buffer::Handle VulkanDevice::createBuffer(Buffer::CreateInfo&& createInfo)
{
    assert(createInfo.initialData.size() <= createInfo.size);
    // High level setup

    if(createInfo.allStates.containsStorageBufferUsage() && createInfo.allStates.containsUniformBufferUsage())
    {
        // TODO: LOGGER: Cant use both, using just storage
        createInfo.allStates.unset(
            ResourceState::UniformBuffer | ResourceState::UniformBufferGraphics |
            ResourceState::UniformBufferCompute);
        assert(!createInfo.allStates.containsUniformBufferUsage());
    }
    if(!createInfo.initialData.empty() && !(createInfo.allStates & ResourceState::TransferDst))
    {
        // TODO: Log: Trying to create buffer with initial data but without transfer dst bit set
        createInfo.allStates |= ResourceState::TransferDst;
    }

    // Low level creation

    VkBuffer vkBuffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
    ResourceIndex resourceIndex;
    void* mappedPtr = nullptr;

    VkBufferCreateInfo bufferCrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,

        .size = createInfo.size,
        .usage = toVkBufferUsage(createInfo.allStates),
    };

    VmaAllocationCreateInfo vmaAllocCrInfo = toVMAAllocCrInfo(createInfo.memoryType);
    VkResult res = vmaCreateBuffer(allocator, &bufferCrInfo, &vmaAllocCrInfo, &vkBuffer, &allocation, &allocInfo);

    if(createInfo.memoryType == Buffer::MemoryType::CPU ||
       createInfo.memoryType == Buffer::MemoryType::GPU_BUT_CPU_VISIBLE)
        mappedPtr = allocInfo.pMappedData;

    assert(res == VK_SUCCESS);

    if(!createInfo.initialData.empty())
    {
        auto stagingAlloc = allocateStagingData(createInfo.initialData.size());
        memcpy(stagingAlloc.ptr, createInfo.initialData.data(), createInfo.initialData.size());

        VkBufferCopy copy{
            .srcOffset = stagingAlloc.offset,
            .dstOffset = 0,
            .size = createInfo.size,
        };
        vkCmdCopyBuffer(
            getCurrentFrameData().uploadCommandBuffer, *get<VkBuffer>(stagingAlloc.buffer), vkBuffer, 1, &copy);

        VkBufferMemoryBarrier2 barrier = toVkBufferMemoryBarrier(VulkanBufferBarrier{
            .buffer = vkBuffer,
            .stateBefore = ResourceState::TransferDst,
            .stateAfter = createInfo.initialState,
        });
        VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier,
        };
        vkCmdPipelineBarrier2(getCurrentFrameData().uploadCommandBuffer, &dependencyInfo);
    }

    if(!createInfo.debugName.empty())
    {
        setDebugName(vkBuffer, createInfo.debugName.data());
    }

    if((createInfo.allStates & ResourceState::UniformBuffer) ||
       (createInfo.allStates & ResourceState::UniformBufferGraphics) ||
       (createInfo.allStates & ResourceState::UniformBufferCompute))
    {
        resourceIndex = bindlessManager.createBufferBinding(vkBuffer, BindlessManager::BufferUsage::Uniform);
    }
    else if(
        (createInfo.allStates & ResourceState::Storage) ||
        (createInfo.allStates & ResourceState::StorageGraphics) ||
        (createInfo.allStates & ResourceState::StorageCompute))
    {
        resourceIndex = bindlessManager.createBufferBinding(vkBuffer, BindlessManager::BufferUsage::Storage);
    }

    // TODO: lock pool?
    Buffer::Handle newHandle = bufferPool.insert(
        createInfo.debugName,
        Buffer::Descriptor{
            .size = createInfo.size,
            .memoryType = createInfo.memoryType,
            .allStates = createInfo.allStates,
        },
        vkBuffer,
        mappedPtr,
        Buffer::Allocation{
            .allocation = allocation,
            .allocInfo = allocInfo,
        },
        resourceIndex //
    );

    return newHandle;
}

void VulkanDevice::destroy(Buffer::Handle handle)
{
    // TODO: Enqueue into a per frame queue

    // its possible that this handle is outdated
    if(!bufferPool.isHandleValid(handle))
    {
        return;
    }

    ResourceStateMulti states = get<Buffer::Descriptor>(handle)->allStates;
    auto resourceIndex = *get<ResourceIndex>(handle);
    if(states &
       (ResourceState::UniformBuffer | ResourceState::UniformBufferCompute | ResourceState::UniformBufferGraphics))
    {
        bindlessManager.freeBufferBinding(resourceIndex, BindlessManager::BufferUsage::Uniform);
    }
    if(states & (ResourceState::Storage | ResourceState::StorageCompute | ResourceState::StorageGraphics))
    {
        bindlessManager.freeBufferBinding(resourceIndex, BindlessManager::BufferUsage::Storage);
    }

    VkBuffer buffer = *get<VkBuffer>(handle);
    VmaAllocation alloc = get<Buffer::Allocation>(handle)->allocation;

    deleteQueue.pushBack([allocator = allocator, buffer = buffer, alloc = alloc]()
                         { vmaDestroyBuffer(allocator, buffer, alloc); });
    bufferPool.remove(handle);
}

Handle<Sampler> VulkanDevice::createSampler(const Sampler::Info& info)
{
    // Check if such a sampler already exists

    // since the sampler limit is quite low atm, and sampler creation should be a rare thing
    // just doing a linear search here. Could of course hash the creation info and do some lookup
    // in the future

    Handle<Sampler> newHandle = samplerPool.find([&](Sampler* sampler) { return sampler->info == info; });
    if(newHandle.isValid())
        return newHandle;

    // Otherwise create a new sampler

    VkSamplerCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = toVkFilter(info.magFilter),
        .minFilter = toVkFilter(info.minFilter),
        .mipmapMode = toVkMipmapMode(info.mipMapFilter),
        .addressModeU = toVkAddressMode(info.addressModeU),
        .addressModeV = toVkAddressMode(info.addressModeV),
        .addressModeW = toVkAddressMode(info.addressModeW),
        .mipLodBias = 0.0f,
        .anisotropyEnable = info.enableAnisotropy,
        .maxAnisotropy = 16.0f, // TODO: retrieve from hardware capabilities
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_LESS,
        .minLod = info.minLod,
        .maxLod = info.maxLod,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VulkanSampler sampler;
    vkCreateSampler(device, &createInfo, nullptr, &sampler.sampler);
    sampler.resourceIndex = bindlessManager.createSamplerBinding(sampler.sampler);

    newHandle = samplerPool.insert(Sampler{
        .info = info,
        .sampler = sampler,
    });

    return newHandle;
}

void VulkanDevice::destroy(Handle<Sampler> sampler)
{
    bindlessManager.freeSamplerBinding(get(sampler)->sampler.resourceIndex);

    VulkanSampler vksampler = get(sampler)->sampler;
    deleteQueue.pushBack([=]() { vkDestroySampler(device, vksampler.sampler, nullptr); });
    samplerPool.remove(sampler);
}

Texture::Handle VulkanDevice::createTexture(Texture::CreateInfo&& createInfo)
{
    // High level setup

    const uint32_t maxDimension = std::max({createInfo.size.width, createInfo.size.height, createInfo.size.depth});
    const uint32_t maxMipLevels = int32_t(floor(log2(maxDimension))) + 1;
    // TODO: warn if clamping to max happened
    int32_t actualMipLevels = createInfo.mipLevels == Texture::MipLevels::All
                                  ? maxMipLevels
                                  : std::min<int32_t>(createInfo.mipLevels, maxMipLevels);
    assert(actualMipLevels > 0);
    createInfo.mipLevels = actualMipLevels;

    if(!createInfo.initialData.empty())
    {
        // TODO: LOG: warn
        createInfo.allStates |= ResourceState::TransferDst;
    }

    if(createInfo.mipLevels > 1)
    {
        // TODO: LOG: warn (needed for automatic mip fill)
        createInfo.allStates |= ResourceState::TransferSrc;
        createInfo.allStates |= ResourceState::TransferDst;
    }

    // Low level creation

    VmaAllocation allocation;
    VkImage image;
    VkImageView view;
    uint32_t resourceIndex;

    VkImageCreateFlags flags = createInfo.type == Texture::Type::tCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    VkImageCreateInfo imageCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,

        .imageType = toVkImgType(createInfo.type),
        .format = toVkFormat(createInfo.format),
        .extent =
            {
                .width = createInfo.size.width,
                .height = createInfo.size.height,
                .depth = createInfo.size.depth,
            },

        .mipLevels = static_cast<uint32_t>(createInfo.mipLevels),
        .arrayLayers = toVkArrayLayers(createInfo.arrayLength, createInfo.type),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = toVkImageUsage(createInfo.allStates),
    };
    VmaAllocationCreateInfo imgAllocInfo{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VkResult res = vmaCreateImage(allocator, &imageCrInfo, &imgAllocInfo, &image, &allocation, nullptr);
    assert(res == VK_SUCCESS);

    if(!createInfo.initialData.empty())
    {
        auto stagingAlloc = allocateStagingData(createInfo.initialData.size());
        memcpy(stagingAlloc.ptr, createInfo.initialData.data(), createInfo.initialData.size());

        VkImageMemoryBarrier2 imgBarrier = toVkImageMemoryBarrier(VulkanImageBarrier{
            .texture = image,
            .descriptor = Texture::Descriptor::fromCreateInfo(createInfo),
            .stateBefore = ResourceState::Undefined,
            .stateAfter = ResourceState::TransferDst,
        });
        VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imgBarrier,
        };
        vkCmdPipelineBarrier2(getCurrentFrameData().uploadCommandBuffer, &dependencyInfo);

        VkBufferImageCopy copyRegion{
            .bufferOffset = stagingAlloc.offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
            .imageExtent =
                {
                    .width = createInfo.size.width,
                    .height = createInfo.size.height,
                    .depth = createInfo.size.depth,
                },
        };

        vkCmdCopyBufferToImage(
            getCurrentFrameData().uploadCommandBuffer,
            *get<VkBuffer>(stagingAlloc.buffer),
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);

        imgBarrier = toVkImageMemoryBarrier(VulkanImageBarrier{
            .texture = image,
            .descriptor = Texture::Descriptor::fromCreateInfo(createInfo),
            .stateBefore = ResourceState::TransferDst,
            .stateAfter = createInfo.initialState,
        });
        dependencyInfo = VkDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imgBarrier,
        };
        vkCmdPipelineBarrier2(getCurrentFrameData().uploadCommandBuffer, &dependencyInfo);

        if(createInfo.fillMipLevels && createInfo.mipLevels > 1)
        {
            fillMipLevels(
                getCurrentFrameData().uploadCommandBuffer,
                image,
                Texture::Descriptor::fromCreateInfo(createInfo),
                createInfo.initialState);
        }
    }
    else if(createInfo.initialState != ResourceState::Undefined)
    {
        // Dont upload anything, just transition
        auto imgBarrier = toVkImageMemoryBarrier(VulkanImageBarrier{
            .texture = image,
            .descriptor = Texture::Descriptor::fromCreateInfo(createInfo),
            .stateBefore = ResourceState::Undefined,
            .stateAfter = createInfo.initialState,
        });
        VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imgBarrier,
        };
        vkCmdPipelineBarrier2(getCurrentFrameData().uploadCommandBuffer, &dependencyInfo);
    }
    setDebugName(image, (createInfo.debugName + "_image").c_str());

    VkImageViewType viewType =
        createInfo.type == Texture::Type::tCube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    VkImageViewCreateInfo imageViewCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = image,
        .viewType = viewType,
        .format = toVkFormat(createInfo.format),
        .components =
            VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = toVkImageAspect(createInfo.format),
                .baseMipLevel = 0,
                .levelCount = static_cast<uint32_t>(createInfo.mipLevels),
                .baseArrayLayer = 0,
                .layerCount = toVkArrayLayers(createInfo.arrayLength, createInfo.type),
            },
    };
    vkCreateImageView(device, &imageViewCrInfo, nullptr, &view);
    setDebugName(view, (createInfo.debugName + "_view").c_str());

    // TODO: containsSamplingState()/...StorageState() functions!
    bool usedForSampling = (createInfo.allStates & ResourceState::SampleSource) ||
                           (createInfo.allStates & ResourceState::SampleSourceGraphics) ||
                           (createInfo.allStates & ResourceState::SampleSourceCompute);
    bool usedForStorage = (createInfo.allStates & ResourceState::Storage) ||
                          (createInfo.allStates & ResourceState::StorageGraphics) ||
                          (createInfo.allStates & ResourceState::StorageCompute);

    if(usedForSampling && usedForStorage)
    {
        resourceIndex = bindlessManager.createImageBinding(view, BindlessManager::ImageUsage::Both);
    }
    // else: not both but maybe one of the two
    else if(usedForSampling)
    {
        resourceIndex = bindlessManager.createImageBinding(view, BindlessManager::ImageUsage::Sampled);
    }
    else if(usedForStorage)
    {
        resourceIndex = bindlessManager.createImageBinding(view, BindlessManager::ImageUsage::Storage);
    }

    Texture::Handle newHandle = texturePool.insert(
        createInfo.debugName,
        Texture::Descriptor::fromCreateInfo(createInfo),
        Texture::GPU{
            .image = image,
            .imageView = view,
        },
        resourceIndex,
        Texture::Allocation{.allocation = allocation} //
    );

    return newHandle;
}

void VulkanDevice::destroy(Texture::Handle handle)
{
    if(!texturePool.isHandleValid(handle))
        return;

    ResourceStateMulti states = get<Texture::Descriptor>(handle)->allStates;
    auto resourceIndex = *get<ResourceIndex>(handle);
    bool usedSampling = bool(
        states &
        (ResourceState::SampleSource | ResourceState::SampleSourceCompute | ResourceState::SampleSourceGraphics));

    bool usedStorage =
        bool(states & (ResourceState::Storage | ResourceState::StorageCompute | ResourceState::StorageGraphics));

    BindlessManager::ImageUsage usage;
    if(usedSampling && usedStorage)
        usage = BindlessManager::ImageUsage::Both;
    else if(usedSampling)
        usage = BindlessManager::ImageUsage::Sampled;
    else if(usedStorage)
        usage = BindlessManager::ImageUsage::Storage;

    bindlessManager.freeImageBinding(resourceIndex, usage);

    VkImageView imageView = get<Texture::GPU>(handle)->imageView;
    deleteQueue.pushBack([=]() { vkDestroyImageView(device, imageView, nullptr); });

    VkImage image = get<Texture::GPU>(handle)->image;
    VmaAllocation vmaAllocation = get<Texture::Allocation>(handle)->allocation;

    deleteQueue.pushBack([=]() { vmaDestroyImage(allocator, image, vmaAllocation); });

    texturePool.remove(handle);
}

Handle<TextureView> VulkanDevice::createTextureView(TextureView::CreateInfo&& createInfo)
{
    auto* parentDesc = get<Texture::Descriptor>(createInfo.parent);

    assert(!(createInfo.allStates & (~parentDesc->allStates)));

    VkImageViewType viewType =
        createInfo.type == Texture::Type::tCube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    VkImageViewCreateInfo imageViewCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = get<Texture::GPU>(createInfo.parent)->image,
        .viewType = viewType,
        .format = toVkFormat(parentDesc->format),
        .components =
            VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = toVkImageAspect(parentDesc->format),
                .baseMipLevel = createInfo.baseMipLevel,
                .levelCount = createInfo.mipCount,
                .baseArrayLayer = createInfo.baseArrayLayer,
                .layerCount =
                    createInfo.type == Texture::Type::tCube ? 6 * createInfo.arrayLength : createInfo.arrayLength,
            },
    };

    VkImageView view;
    uint32_t resourceIndex;
    vkCreateImageView(device, &imageViewCrInfo, nullptr, &view);
    setDebugName(view, createInfo.debugName.c_str());

    bool usedForSampling = (createInfo.allStates & ResourceState::SampleSource) ||
                           (createInfo.allStates & ResourceState::SampleSourceGraphics) ||
                           (createInfo.allStates & ResourceState::SampleSourceCompute);
    bool usedForStorage = (createInfo.allStates & ResourceState::Storage) ||
                          (createInfo.allStates & ResourceState::StorageGraphics) ||
                          (createInfo.allStates & ResourceState::StorageCompute);

    if(usedForSampling && usedForStorage)
    {
        resourceIndex = bindlessManager.createImageBinding(view, BindlessManager::ImageUsage::Both);
    }
    // else: not both but maybe one of the two
    else if(usedForSampling)
    {
        resourceIndex = bindlessManager.createImageBinding(view, BindlessManager::ImageUsage::Sampled);
    }
    else if(usedForStorage)
    {
        resourceIndex = bindlessManager.createImageBinding(view, BindlessManager::ImageUsage::Storage);
    }

    Handle<TextureView> newHandle = textureViewPool.insert(TextureView{
        .descriptor =
            {
                .parent = createInfo.parent,
                .type = createInfo.type,
                .allStates = createInfo.allStates,
                .baseMipLevel = createInfo.baseMipLevel,
                .mipCount = createInfo.mipCount,
                .baseArrayLayer = createInfo.baseArrayLayer,
                .arrayLength = createInfo.arrayLength,
            },
        .imageView = view,
        .resourceIndex = resourceIndex,
    });

    return newHandle;
}

void VulkanDevice::destroy(Handle<TextureView> handle)
{
    TextureView* view = textureViewPool.get(handle);
    deleteQueue.pushBack([device = device, view = view->imageView]()
                         { vkDestroyImageView(device, view, nullptr); });
    textureViewPool.remove(handle);
}

VkPipeline VulkanDevice::createGraphicsPipeline(PipelineCreateInfo&& createInfo)
{
    // (temp) Shader Modules ----------------
    VkShaderModule vertSM;
    VkShaderModuleCreateInfo vertSMCrInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = 4u * createInfo.vertexSpirv.size(), // size in bytes, but spirvBinary is uint32 vector!
        .pCode = createInfo.vertexSpirv.data(),
    };
    if(vkCreateShaderModule(device, &vertSMCrInfo, nullptr, &vertSM) != VK_SUCCESS)
    {
        assert(false);
    }

    VkShaderModule fragSM;
    VkShaderModuleCreateInfo fragSMCrInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = 4u * createInfo.fragmentSpirv.size(), // size in bytes, but spirvBinary is uint32 vector!
        .pCode = createInfo.fragmentSpirv.data(),
    };
    if(vkCreateShaderModule(device, &fragSMCrInfo, nullptr, &fragSM) != VK_SUCCESS)
    {
        assert(false);
    }

    // todo: check if pipeline with given parameters already exists, and just return that in case it does!
    //       Though im not sure if that should happen on this level, or as part of the application level
    //       Especially since it would require something like the spirv path to check equality

    // Creating the pipeline ---------------------------

    const VkPipelineShaderStageCreateInfo shaderStageCrInfos[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,

            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertSM,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,

            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragSM,
            .pName = "main",
        },
    };

    const VertexInputDescription vertexDescription = VertexInputDescription::getDefault();
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,

        .vertexBindingDescriptionCount = (uint32_t)vertexDescription.bindings.size(),
        .pVertexBindingDescriptions = vertexDescription.bindings.data(),
        .vertexAttributeDescriptionCount = (uint32_t)vertexDescription.attributes.size(),
        .pVertexAttributeDescriptions = vertexDescription.attributes.data(),
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,

        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,

        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,

        .polygonMode = VK_POLYGON_MODE_FILL,

        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,

        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,

        .lineWidth = 1.0f,
    };

    const bool depthTest = true;
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,

        .depthTestEnable = depthTest ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = depthTest ? VK_TRUE : VK_FALSE, // TODO: decouple
        .depthCompareOp = depthTest ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCrInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,

        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | //
                          VK_COLOR_COMPONENT_G_BIT | //
                          VK_COLOR_COMPONENT_B_BIT | //
                          VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineViewportStateCreateInfo viewportStateCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,

        .viewportCount = 1,
        .scissorCount = 1,
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,

        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,

        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachmentState,
    };

    constexpr VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ArraySize(dynamicStates),
        .pDynamicStates = &dynamicStates[0],
    };

    std::vector<VkFormat> colorFormats{createInfo.colorFormats.size()};
    for(int i = 0; i < colorFormats.size(); i++)
    {
        colorFormats[i] = toVkFormat(createInfo.colorFormats[i]);
    }

    const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
        .pColorAttachmentFormats = colorFormats.data(),
        .depthAttachmentFormat = toVkFormat(createInfo.depthFormat),
        .stencilAttachmentFormat = toVkFormat(createInfo.stencilFormat),
    };

    const VkGraphicsPipelineCreateInfo pipelineCrInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipelineRenderingCreateInfo,

        .stageCount = 2,
        .pStages = &shaderStageCrInfos[0],
        .pVertexInputState = &vertexInputStateCrInfo,
        .pInputAssemblyState = &inputAssemblyStateCrInfo,
        .pViewportState = &viewportStateCrInfo,
        .pRasterizationState = &rasterizationStateCrInfo,
        .pMultisampleState = &multisampleStateCrInfo,
        .pDepthStencilState = &depthStencilStateCrInfo,
        .pColorBlendState = &colorBlendStateCrInfo,
        .pDynamicState = &dynamicState,

        .layout = bindlessPipelineLayout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    VkPipeline pipeline;
    if(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCrInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        assert(false);
    }
    setDebugName(pipeline, createInfo.debugName.data());

    vkDestroyShaderModule(device, vertSM, nullptr);
    vkDestroyShaderModule(device, fragSM, nullptr);

    return pipeline;
}

VkPipeline VulkanDevice::createComputePipeline(Span<uint32_t> spirv, std::string_view debugName)
{
    // (temp) Shader Modules ----------------
    VkShaderModule tempSM;
    VkShaderModuleCreateInfo SMCrInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = 4u * spirv.size(), // size in bytes, but spirvBinary is uint32 vector!
        .pCode = spirv.data(),
    };
    if(vkCreateShaderModule(device, &SMCrInfo, nullptr, &tempSM) != VK_SUCCESS)
    {
        assert(false);
    }

    // todo: check if pipeline with given parameters already exists, and just return that in case it does!
    //       Though im not sure if that should happen on this level, or as part of the application level
    //       Especially since it would require something like the spirv path to check equality

    // Creating the pipeline ---------------------------
    VkPipeline pipeline;
    const VkPipelineShaderStageCreateInfo shaderStageCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = tempSM,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };

    const VkComputePipelineCreateInfo pipelineCrInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shaderStageCrInfo,
        .layout = bindlessPipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    if(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCrInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        assert(false);
    }
    setDebugName(pipeline, (std::string{debugName}).c_str());

    vkDestroyShaderModule(device, tempSM, nullptr);

    return pipeline;
}

void VulkanDevice::destroy(VkPipeline pipeline)
{
    if(pipeline != VK_NULL_HANDLE)
    {
        deleteQueue.pushBack([=]() { vkDestroyPipeline(device, pipeline, nullptr); });
    }
}

GPUAllocation VulkanDevice::allocateStagingData(size_t size)
{
    auto& currentFrameData = getCurrentFrameData();
    return currentFrameData.stagingAllocator.allocate(size);
}

void VulkanDevice::startInitializationWork()
{
    auto& curFrameData = getCurrentFrameData();

    assertVkResult(vkWaitForFences(device, 1, &curFrameData.commandsDone, true, UINT64_MAX));
    assertVkResult(vkResetFences(device, 1, &curFrameData.commandsDone));
    curFrameData.stagingAllocator.reset();

    // no commmand buffers / pools to clear / reset yet

    // begin upload command buffer
    {
        VkCommandBufferBeginInfo cmdBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,

            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        assertVkResult(vkBeginCommandBuffer(curFrameData.uploadCommandBuffer, &cmdBeginInfo));
    }
}

void VulkanDevice::submitInitializationWork(Span<const VkCommandBuffer> cmdBuffersToSubmit)
{
    inInitialization = false;

    auto& curFrameData = getCurrentFrameData();

    vkEndCommandBuffer(curFrameData.uploadCommandBuffer);
    std::vector<VkCommandBuffer> buffers{cmdBuffersToSubmit.size() + 1};
    std::copy(cmdBuffersToSubmit.begin(), cmdBuffersToSubmit.end(), ++buffers.begin());
    buffers[0] = curFrameData.uploadCommandBuffer;

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,

        .waitSemaphoreCount = 0,

        .commandBufferCount = static_cast<uint32_t>(buffers.size()),
        .pCommandBuffers = buffers.data(),

        .signalSemaphoreCount = 0,
    };

    assertVkResult(vkQueueSubmit(graphicsAndComputeQueue, 1, &submitInfo, curFrameData.commandsDone));
}

void VulkanDevice::startNextFrame()
{
    assert(inInitialization == false && "Initialization frame has not been ended yet!");

    frameNumber++;

    auto& curFrameData = getCurrentFrameData();

    assertVkResult(vkWaitForFences(device, 1, &curFrameData.commandsDone, true, UINT64_MAX));
    assertVkResult(vkResetFences(device, 1, &curFrameData.commandsDone));
    curFrameData.stagingAllocator.reset();

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
    assertVkResult(vkResetCommandPool(device, curFrameData.uploadCommandPool, 0));

    // begin upload command buffer
    {
        VkCommandBufferBeginInfo cmdBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,

            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        assertVkResult(vkBeginCommandBuffer(curFrameData.uploadCommandBuffer, &cmdBeginInfo));
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
void VulkanDevice::endCommandBuffer(VkCommandBuffer cmd) { vkEndCommandBuffer(cmd); }

void VulkanDevice::submitCommandBuffers(Span<const VkCommandBuffer> cmdBuffers)
{
    auto& curFrameData = getCurrentFrameData();

    vkEndCommandBuffer(curFrameData.uploadCommandBuffer);
    std::vector<VkCommandBuffer> buffers{cmdBuffers.size() + 1};
    std::copy(cmdBuffers.begin(), cmdBuffers.end(), ++buffers.begin());
    buffers[0] = curFrameData.uploadCommandBuffer;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curFrameData.swapchainImageAvailable,
        .pWaitDstStageMask = &waitStage,

        .commandBufferCount = static_cast<uint32_t>(buffers.size()),
        .pCommandBuffers = buffers.data(),

        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &curFrameData.swapchainImageRenderFinished,
    };

    assertVkResult(vkQueueSubmit(graphicsAndComputeQueue, 1, &submitInfo, curFrameData.commandsDone));
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

void VulkanDevice::fillMipLevels(VkCommandBuffer cmd, Texture::Handle texture, ResourceState state)
{
    return fillMipLevels(cmd, get<Texture::GPU>(texture)->image, *get<Texture::Descriptor>(texture), state);
}

void VulkanDevice::copyBuffer(VkCommandBuffer cmd, Buffer::Handle src, Buffer::Handle dest)
{
    size_t bufSize = get<Buffer::Descriptor>(src)->size;
    copyBuffer(cmd, src, 0, dest, 0, bufSize);
}
void VulkanDevice::copyBuffer(
    VkCommandBuffer cmd, Buffer::Handle src, size_t srcOffset, Buffer::Handle dest, size_t destOffset, size_t size)
{
    VkBufferCopy copyRegion{
        srcOffset,
        destOffset,
        size,
    };
    vkCmdCopyBuffer(cmd, *get<VkBuffer>(src), *get<VkBuffer>(dest), 1, &copyRegion);
}

void VulkanDevice::dispatchCompute(VkCommandBuffer cmd, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);
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
    VkCommandBuffer cmd, Span<const ColorTarget>&& colorTargets, const DepthTarget&& depthTarget)
{
    VkClearValue clearValue{.color = {1.0f, 1.0f, 1.0f, 1.0f}};
    VkClearValue depthStencilClear{.depthStencil = {.depth = 1.0f, .stencil = 0u}};

    std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos;
    colorAttachmentInfos.reserve(colorTargets.size());
    for(const auto& target : colorTargets)
    {
        const VkImageView view = std::holds_alternative<Texture::Handle>(target.texture)
                                     ? get<Texture::GPU>(std::get<Texture::Handle>(target.texture))->imageView
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

    bool hasDepthAttachment = depthTarget.texture.isValid();
    VkRenderingAttachmentInfo depthAttachmentInfo;
    if(hasDepthAttachment)
        depthAttachmentInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = get<Texture::GPU>(depthTarget.texture)->imageView,
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

void VulkanDevice::endRendering(VkCommandBuffer cmd) { vkCmdEndRendering(cmd); }

void VulkanDevice::insertBarriers(VkCommandBuffer cmd, Span<const Barrier> barriers)
{
    std::vector<VkImageMemoryBarrier2> imageBarriers;
    std::vector<VkBufferMemoryBarrier2> bufferBarriers;

    for(const auto& barrier : barriers)
    {
        if(barrier.type == Barrier::Type::Buffer)
        {
            if(!bufferPool.isHandleValid(barrier.buffer.buffer))
            {
                BREAKPOINT;
                continue; // TODO: LOG warning
            }
            auto* descriptor = get<Buffer::Descriptor>(barrier.buffer.buffer);
            VkBufferMemoryBarrier2 vkBarrier{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .pNext = nullptr,

                .srcStageMask = toVkPipelineStage(barrier.buffer.stateBefore),
                .srcAccessMask = toVkAccessFlags(barrier.buffer.stateBefore),

                .dstStageMask = toVkPipelineStage(barrier.buffer.stateAfter),
                .dstAccessMask = toVkAccessFlags(barrier.buffer.stateAfter),

                .srcQueueFamilyIndex = graphicsAndComputeQueueFamily,
                .dstQueueFamilyIndex = graphicsAndComputeQueueFamily,

                .buffer = *get<VkBuffer>(barrier.buffer.buffer),
                .offset = barrier.buffer.offset,
                .size = barrier.buffer.size,
            };

            bufferBarriers.emplace_back(vkBarrier);
        }
        else if(barrier.type == Barrier::Type::Image)
        {
            const auto* descriptor = get<Texture::Descriptor>(barrier.image.texture);
            if(descriptor == nullptr)
            {
                BREAKPOINT;
                continue; // TODO: LOG warning
            }
            assert(descriptor->mipLevels > 0);
            int32_t mipCount = barrier.image.mipCount == Texture::MipLevels::All
                                   ? descriptor->mipLevels - barrier.image.mipLevel
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

                .image = get<Texture::GPU>(barrier.image.texture)->image,
                .subresourceRange =
                    {
                        .aspectMask = toVkImageAspect(descriptor->format),
                        .baseMipLevel = static_cast<uint32_t>(barrier.image.mipLevel),
                        .levelCount = static_cast<uint32_t>(mipCount),
                        .baseArrayLayer = static_cast<uint32_t>(barrier.image.arrayLayer),
                        .layerCount = static_cast<uint32_t>(
                            descriptor->type == Texture::Type::tCube ? 6 * barrier.image.arrayLength
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

void VulkanDevice::setGraphicsPipelineState(VkCommandBuffer cmd, VkPipeline pipe)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
}

void VulkanDevice::setComputePipelineState(VkCommandBuffer cmd, VkPipeline pipe)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
}

// TODO: CORRECT PUSH CONSTANT RANGES FOR COMPUTE PIPELINES !!!!!
void VulkanDevice::pushConstants(VkCommandBuffer cmd, size_t size, void* data, size_t offset)
{
    vkCmdPushConstants(cmd, bindlessPipelineLayout, VK_SHADER_STAGE_ALL, offset, size, data);
}

void VulkanDevice::bindIndexBuffer(VkCommandBuffer cmd, Buffer::Handle buffer, size_t offset)
{
    vkCmdBindIndexBuffer(cmd, *get<VkBuffer>(buffer), offset, VK_INDEX_TYPE_UINT32);
}

void VulkanDevice::bindVertexBuffers(
    VkCommandBuffer cmd,
    uint32_t startBinding,
    uint32_t count,
    Span<const Buffer::Handle> buffers,
    Span<const uint64_t> offsets)
{
    assert(buffers.size() == offsets.size());
    std::vector<VkBuffer> vkBuffers{buffers.size()};
    for(int i = 0; i < buffers.size(); i++)
    {
        vkBuffers[i] = *get<VkBuffer>(buffers[i]);
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

void VulkanDevice::drawImGui(VkCommandBuffer cmd) { ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd); }

void VulkanDevice::disableValidationErrorBreakpoint() { breakOnValidationError = false; }
void VulkanDevice::enableValidationErrorBreakpoint() { breakOnValidationError = true; }

void VulkanDevice::startDebugRegion(VkCommandBuffer cmd, const char* name)
{
    VkDebugUtilsLabelEXT info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = name,
    };
    pfnCmdBeginDebugUtilsLabelEXT(cmd, &info);
}
void VulkanDevice::endDebugRegion(VkCommandBuffer cmd) { pfnCmdEndDebugUtilsLabelEXT(cmd); }

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

void VulkanDevice::fillMipLevels(
    VkCommandBuffer cmd, VkImage image, const Texture::Descriptor& descriptor, ResourceState state)
{
    /*
        TODO: check that image was created with usage_transfer_src_bit !
              Switch to compute shader based solution? Could get rid of needing to mark
              *all* textures as transfer_src/dst, just because mips are needed.
              But requires a lot more work to handle different texture types (3d, cube, array)
    */

    if(descriptor.mipLevels == 1)
        return;

    uint32_t startWidth = descriptor.size.width;
    uint32_t startHeight = descriptor.size.height;
    uint32_t startDepth = descriptor.size.depth;

    // Transition mip 0 to transfer source
    if(state != ResourceState::TransferSrc)
    {
        VkImageMemoryBarrier2 imgBarrier = toVkImageMemoryBarrier(VulkanImageBarrier{
            .texture = image,
            .descriptor = descriptor,
            .stateBefore = state,
            .stateAfter = ResourceState::TransferSrc,
            .mipLevel = 0,
            .mipCount = 1,
            .arrayLength = int32_t(descriptor.arrayLength),
        });
        VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imgBarrier,
        };
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    // Generate mip chain
    // Downscaling from each level successively, but could also downscale mip 0 -> mip N each time

    for(int i = 1; i < descriptor.mipLevels; i++)
    {
        // prepare current level to be transfer dst
        if(state != ResourceState::TransferDst)
        {
            VkImageMemoryBarrier2 imgBarrier = toVkImageMemoryBarrier(VulkanImageBarrier{
                .texture = image,
                .descriptor = descriptor,
                .stateBefore = state,
                .stateAfter = ResourceState::TransferDst,
                .mipLevel = i,
                .mipCount = 1,
                .arrayLength = int32_t(descriptor.arrayLength),
            });
            VkDependencyInfo dependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &imgBarrier,
            };
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
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
                    .layerCount = toVkArrayLayers(descriptor.arrayLength, descriptor.type),
                },
            .srcOffsets =
                {{.x = 0, .y = 0, .z = 0}, {int32_t(lastWidth), int32_t(lastHeight), int32_t(lastDepth)}},
            .dstSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = uint32_t(i),
                    .baseArrayLayer = 0,
                    .layerCount = toVkArrayLayers(descriptor.arrayLength, descriptor.type),
                },
            .dstOffsets = {{.x = 0, .y = 0, .z = 0}, {int32_t(curWidth), int32_t(curHeight), int32_t(curDepth)}},
        };

        // do the blit
        vkCmdBlitImage(
            cmd,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR);

        // prepare current level to be transfer src for next mip
        VkImageMemoryBarrier2 imgBarrier = toVkImageMemoryBarrier(VulkanImageBarrier{
            .texture = image,
            .descriptor = descriptor,
            .stateBefore = ResourceState::TransferDst,
            .stateAfter = ResourceState::TransferSrc,
            .mipLevel = i,
            .mipCount = 1,
            .arrayLength = int32_t(descriptor.arrayLength),
        });
        VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imgBarrier,
        };
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    // after the loop, transfer all mips to input state
    VkImageMemoryBarrier2 imgBarrier = toVkImageMemoryBarrier(VulkanImageBarrier{
        .texture = image,
        .descriptor = descriptor,
        .stateBefore = ResourceState::TransferSrc,
        .stateAfter = state,
        .mipLevel = 0,
        .mipCount = descriptor.mipLevels,
        .arrayLength = int32_t(descriptor.arrayLength),
    });
    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imgBarrier,
    };
    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
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

void VulkanDevice::LinearAllocator::reset() { offset = 0; }
GPUAllocation VulkanDevice::LinearAllocator::allocate(size_t size)
{
    if(offset + size > capacity)
    {
        // TODO: could handle special cases where the requested size is like >=80% of the staging buffer capacity!
        //       in that case, could just return a dedicated allocation for just that request instead of using the
        //       normal allocation mechanism

        // enqueue current buffer for deletion
        VulkanDevice::impl()->destroy(this->buffer);

        // create new buffer

        // TODO: be smarter?
        size_t newSize = std::max(this->capacity, size);

        this->buffer = VulkanDevice::impl()->createBuffer(Buffer::CreateInfo{
            .debugName = "StagingBufferReplacement",
            .size = newSize,
            .memoryType = Buffer::MemoryType::CPU,
            .allStates = ResourceState::TransferSrc,
            .initialState = ResourceState::TransferSrc,
        });
        this->capacity = newSize;
        this->offset = 0;
        this->ptr = static_cast<uint8_t*>(*VulkanDevice::impl()->get<void*>(this->buffer));
    }
    // TODO: alignment?
    size_t oldOffset = offset.fetch_add(size);
    return GPUAllocation{.buffer = buffer, .offset = oldOffset, .size = size, .ptr = &(ptr[oldOffset])};
}