#include "VulkanRenderer.hpp"
#include "Graphics/VulkanDebug.hpp"
#include "Graphics/VulkanRenderer.hpp"
#include "Mesh.hpp"
#include "VulkanDebug.hpp"
#include "VulkanDeviceFinder.hpp"
#include "VulkanInit.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanSwapchainSetup.hpp"
#include "VulkanTypes.hpp"

#include "Datastructures/Span.hpp"

#include <fstream>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <stdexcept>
#include <string>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <vulkan/vulkan_core.h>

void VulkanRenderer::init()
{
    // Init window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(windowExtent.width, windowExtent.height, "Vulkan Test", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);

    initVulkan();

    initSwapchain();

    initCommands();

    initDefaultRenderpass();

    initFramebuffers();

    initSyncStructures();

    initDescriptors();

    initPipelines();

    loadMeshes();

    initScene();

    // everything went fine
    isInitialized = true;
}

void VulkanRenderer::initVulkan()
{
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    instance = createInstance(enableValidationLayers, validationLayers);

    if(enableValidationLayers)
        debugMessenger = setupDebugMessenger(instance);

    if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

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
    VulkanSwapchainSetup swapchainSetup(physicalDevice, device, window, surface);
    swapchainSetup.setup(queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value());

    swapchain = swapchainSetup.getSwapchain();
    swapchainExtent = swapchainSetup.getSwapchainExtent();
    swapchainImageFormat = swapchainSetup.getSwapchainImageFormat();
    swapchainImages = swapchainSetup.getSwapchainImages();
    swapchainImageViews = swapchainSetup.createSwapchainImageViews();

    // Depth image
    VkExtent3D depthImageExtent{
        .width = swapchainExtent.width,
        .height = swapchainExtent.height,
        .depth = 1,
    };
    depthFormat = VK_FORMAT_D32_SFLOAT;
    VkImageCreateInfo depthImgCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,

        .imageType = VK_IMAGE_TYPE_2D,

        .format = depthFormat,
        .extent = depthImageExtent,

        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    };
    VmaAllocationCreateInfo depthImgAllocCrInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    vmaCreateImage(
        allocator, &depthImgCrInfo, &depthImgAllocCrInfo, &depthImage.image, &depthImage.allocation, nullptr);
    VkImageViewCreateInfo depthImgViewCrInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,

        .image = depthImage.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,

        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    assertVkResult(vkCreateImageView(device, &depthImgViewCrInfo, nullptr, &depthImageView));

    deleteQueue.pushBack(
        [=]()
        {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            vkDestroyImageView(device, depthImageView, nullptr);
            vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
        });
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
}

void VulkanRenderer::initDefaultRenderpass()
{
    VkAttachmentDescription colorAttachment{
        .format = swapchainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,

        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription depthAttachment{
        .flags = 0,
        .format = depthFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // Only _DEPTH_OPTIMAL ?
    };
    VkAttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    // https://www.reddit.com/r/vulkan/comments/s80reu/comment/hth2uj9/?utm_source=share&utm_medium=web2x&context=3
    VkSubpassDependency colorDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkSubpassDependency depthDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkSubpassDescription subpassDesc{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef,
    };

    VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};
    VkSubpassDependency dependencies[2] = {colorDependency, depthDependency};

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,

        .attachmentCount = 2,
        .pAttachments = &attachments[0],

        .subpassCount = 1,
        .pSubpasses = &subpassDesc,

        .dependencyCount = 2,
        .pDependencies = &dependencies[0],
    };

    assertVkResult(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

    deleteQueue.pushBack([=]() { vkDestroyRenderPass(device, renderPass, nullptr); });
}

void VulkanRenderer::initFramebuffers()
{
    VkFramebufferCreateInfo fbInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,

        .renderPass = renderPass,
        .attachmentCount = 1,
        .width = swapchainExtent.width,
        .height = swapchainExtent.height,
        .layers = 1,
    };

    const uint32_t swapchainImageCount = swapchainImages.size();
    framebuffers.resize(swapchainImageCount);

    for(int i = 0; i < swapchainImageCount; i++)
    {
        VkImageView attachments[2] = {swapchainImageViews[i], depthImageView};

        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = &attachments[0];
        assertVkResult(vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]));

        deleteQueue.pushBack(
            [=]()
            {
                vkDestroyFramebuffer(device, framebuffers[i], nullptr);
                vkDestroyImageView(device, swapchainImageViews[i], nullptr);
            });
    }
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

        assertVkResult(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].presentSemaphore));
        assertVkResult(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore));

        deleteQueue.pushBack(
            [=]()
            {
                vkDestroySemaphore(device, frames[i].presentSemaphore, nullptr);
                vkDestroySemaphore(device, frames[i].renderSemaphore, nullptr);
            });
    }
}

void VulkanRenderer::initDescriptors()
{
    std::vector<VkDescriptorPoolSize> sizes = {
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 10},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 10},
    };
    VkDescriptorPoolCreateInfo poolCrInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .maxSets = 10,
        .poolSizeCount = (uint32_t)sizes.size(),
        .pPoolSizes = sizes.data(),
    };
    vkCreateDescriptorPool(device, &poolCrInfo, nullptr, &descriptorPool);

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
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
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

    const size_t sceneParamBufferSize = FRAMES_IN_FLIGHT * padUniformBufferSize(sizeof(GPUSceneData));
    sceneParameterBuffer =
        createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        frames[i].cameraBuffer =
            createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        const int MAX_OBJECTS = 10000;
        frames[i].objectBuffer = createBuffer(
            sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,

            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &globalSetLayout,
        };
        vkAllocateDescriptorSets(device, &allocInfo, &frames[i].globalDescriptor);

        VkDescriptorBufferInfo camBufferInfo{
            .buffer = frames[i].cameraBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUCameraData),
        };

        VkDescriptorBufferInfo sceneBufferInfo{
            .buffer = sceneParameterBuffer.buffer,
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
            .buffer = frames[i].objectBuffer.buffer,
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
        [=]() { vmaDestroyBuffer(allocator, sceneParameterBuffer.buffer, sceneParameterBuffer.allocation); });
    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        deleteQueue.pushBack(
            [=]()
            {
                vmaDestroyBuffer(allocator, frames[i].cameraBuffer.buffer, frames[i].cameraBuffer.allocation);
                vmaDestroyBuffer(allocator, frames[i].objectBuffer.buffer, frames[i].objectBuffer.allocation);
            });
    }

    deleteQueue.pushBack(
        [=]()
        {
            vkDestroyDescriptorSetLayout(device, globalSetLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, objectSetLayout, nullptr);
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        });
}

void VulkanRenderer::initPipelines()
{
    // Mesh Pipeline
    VkShaderModule meshTriVertShader;
    if(!loadShaderModule("../shaders/tri_mesh.vert.spirv", &meshTriVertShader))
    {
        std::cout << "Error when building the triangle vertex shader module" << std::endl;
    }
    VkShaderModule fragShader;
    if(!loadShaderModule("../shaders/default_lit.frag.spirv", &fragShader))
    {
        std::cout << "Error when building the triangle fragment shader module" << std::endl;
    }

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(MeshPushConstants),
    };
    VkDescriptorSetLayout setLayouts[] = {globalSetLayout, objectSetLayout};
    VkPipelineLayout meshPipelineLayout;
    VkPipelineLayoutCreateInfo meshPipelineLayoutCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .setLayoutCount = 2,
        .pSetLayouts = &setLayouts[0],
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };
    assertVkResult(vkCreatePipelineLayout(device, &meshPipelineLayoutCrInfo, nullptr, &meshPipelineLayout));

    VertexInputDescription vertexDescription = Vertex::getVertexDescription();

    VulkanPipeline meshPipelineWrapper{VulkanPipeline::CreateInfo{
        .shaderStages =
            {{.stage = VK_SHADER_STAGE_VERTEX_BIT, .module = meshTriVertShader},
             {.stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragShader}},
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .viewport =
            {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)swapchainExtent.width,
                .height = (float)swapchainExtent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            },
        .scissor = {.offset = {0, 0}, .extent = swapchainExtent},
        .depthTest = true,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .pipelineLayout = meshPipelineLayout,
    }};
    // TODO: add a nice wrapper for this to pipeline constructor!
    meshPipelineWrapper.vertexInputStateCrInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();
    meshPipelineWrapper.vertexInputStateCrInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    meshPipelineWrapper.vertexInputStateCrInfo.vertexAttributeDescriptionCount =
        vertexDescription.attributes.size();
    meshPipelineWrapper.vertexInputStateCrInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();

    VkPipeline meshPipeline = meshPipelineWrapper.createPipeline(device, renderPass);

    createMaterial(meshPipeline, meshPipelineLayout, "defaultMesh");

    // Destroy these here already. (dont like though since VulkanPipeline object still exists and references
    // these!!, so could cause trouble in future when Pipeline needs to get recreated or something)
    vkDestroyShaderModule(device, meshTriVertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    deleteQueue.pushBack(
        [=]()
        {
            vkDestroyPipeline(device, meshPipeline, nullptr);
            vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);
        });
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

    glfwDestroyWindow(window);
    glfwTerminate();
}

void VulkanRenderer::run()
{
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        draw();
    }

    vkDeviceWaitIdle(device);
}

void VulkanRenderer::draw()
{
    const auto& curFrameData = getCurrentFrameData();
    assertVkResult(vkWaitForFences(device, 1, &curFrameData.renderFence, true, UINT64_MAX));
    assertVkResult(vkResetFences(device, 1, &curFrameData.renderFence));

    uint32_t swapchainImageIndex;
    assertVkResult(vkAcquireNextImageKHR(
        device, swapchain, UINT64_MAX, curFrameData.presentSemaphore, nullptr, &swapchainImageIndex));

    assertVkResult(vkResetCommandBuffer(curFrameData.mainCommandBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,

        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    assertVkResult(vkBeginCommandBuffer(curFrameData.mainCommandBuffer, &cmdBeginInfo));

    float flash = abs(sin(frameNumber / 120.0f));
    VkClearValue clearValue{.color = {0.0f, 0.0f, flash, 1.0f}};
    VkClearValue depthClear{.depthStencil = {.depth = 1.0f}};

    VkClearValue clearValues[2] = {clearValue, depthClear};

    VkRenderPassBeginInfo renderpassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,

        .renderPass = renderPass,
        .framebuffer = framebuffers[swapchainImageIndex],
        .renderArea =
            {
                .offset = {.x = 0, .y = 0},
                .extent = swapchainExtent,
            },
        .clearValueCount = 2,
        .pClearValues = &clearValues[0],
    };

    vkCmdBeginRenderPass(curFrameData.mainCommandBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // todo: sort before
    drawObjects(curFrameData.mainCommandBuffer, renderables.data(), renderables.size());

    vkCmdEndRenderPass(curFrameData.mainCommandBuffer);

    assertVkResult(vkEndCommandBuffer(curFrameData.mainCommandBuffer));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curFrameData.presentSemaphore,
        .pWaitDstStageMask = &waitStage,

        .commandBufferCount = 1,
        .pCommandBuffers = &curFrameData.mainCommandBuffer,

        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &curFrameData.renderSemaphore,
    };

    assertVkResult(vkQueueSubmit(graphicsQueue, 1, &submitInfo, curFrameData.renderFence));

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curFrameData.renderSemaphore,

        .swapchainCount = 1,
        .pSwapchains = &swapchain,

        .pImageIndices = &swapchainImageIndex,
    };

    assertVkResult(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    frameNumber++;
}

bool VulkanRenderer::loadShaderModule(const char* filePath, VkShaderModule* outShaderModule)
{
    // todo: readBinaryFile function, overloaded to either return or fill
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if(!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    auto fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    VkShaderModuleCreateInfo smCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data()),
    };

    VkShaderModule shaderModule;
    if(vkCreateShaderModule(device, &smCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return false;
    }

    *outShaderModule = shaderModule;

    return true;
}

void VulkanRenderer::loadMeshes()
{
    Mesh triangleMesh;
    triangleMesh.vertices.resize(3);

    triangleMesh.vertices[0].position = {1.f, 1.f, 0.0f};
    triangleMesh.vertices[1].position = {-1.f, 1.f, 0.0f};
    triangleMesh.vertices[2].position = {0.f, -1.f, 0.0f};

    triangleMesh.vertices[0].color = {0.0f, 1.0f, 0.0f};
    triangleMesh.vertices[1].color = {0.0f, 1.0f, 0.0f};
    triangleMesh.vertices[2].color = {0.0f, 1.0f, 0.0f};

    // normals...

    uploadMesh(triangleMesh);

    // load monkey mesh
    Mesh monkeyMesh;
    monkeyMesh.loadFromObj(ASSETS_PATH "/vkguide/monkey_smooth.obj");

    uploadMesh(monkeyMesh);

    meshes["monkey"] = monkeyMesh;
    meshes["triangle"] = triangleMesh;
}

void VulkanRenderer::uploadMesh(Mesh& mesh)
{
    VkBufferCreateInfo bufferCrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = mesh.vertices.size() * sizeof(Vertex),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };

    // todo: switch to correct usage of Auto + correct bitflags!
    // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/quick_start.html#quick_start_resource_allocation:~:text=(%26allocatorCreateInfo%2C%20%26allocator)%3B-,Resource%20allocation,-When%20you%20want
    // Flags parameter:
    // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/group__group__alloc.html#gad9889c10c798b040d59c92f257cae597
    // Usage parameter:
    // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/group__group__alloc.html#gaa5846affa1e9da3800e3e78fae2305cc
    VmaAllocationCreateInfo vmaallocCrInfo{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };

    assertVkResult(vmaCreateBuffer(
        allocator,
        &bufferCrInfo,
        &vmaallocCrInfo,
        &mesh.vertexBuffer.buffer,
        &mesh.vertexBuffer.allocation,
        nullptr));

    deleteQueue.pushBack([=]()
                         { vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation); });

    void* data = nullptr;
    vmaMapMemory(allocator, mesh.vertexBuffer.allocation, &data);
    memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator, mesh.vertexBuffer.allocation);
}

void VulkanRenderer::initScene()
{
    const auto& newRenderable = renderables.emplace_back(RenderObject{
        .mesh = getMesh("monkey"),
        .material = getMaterial("defaultMesh"),
        .transformMatrix = glm::mat4{1.0f},
    });
    assert(newRenderable.mesh != nullptr);
    assert(newRenderable.material != nullptr);

    for(int x = -20; x <= 20; x++)
    {
        for(int y = -20; y <= 20; y++)
        {
            glm::mat4 translation = glm::translate(glm::vec3{x, 0, y});
            glm::mat4 scale = glm::scale(glm::vec3{0.2f});

            const auto& newRenderable = renderables.emplace_back(RenderObject{
                .mesh = getMesh("triangle"),
                .material = getMaterial("defaultMesh"),
                .transformMatrix = translation * scale,
            });
            assert(newRenderable.mesh != nullptr);
            assert(newRenderable.material != nullptr);
        }
    }
}

Material* VulkanRenderer::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
    Material mat{
        .pipeline = pipeline,
        .pipelineLayout = layout,
    };
    materials[name] = mat;
    return &materials[name];
}

Material* VulkanRenderer::getMaterial(const std::string& name)
{
    auto it = materials.find(name);
    if(it == materials.end())
        return nullptr;
    return &(it->second);
}

Mesh* VulkanRenderer::getMesh(const std::string& name)
{
    auto it = meshes.find(name);
    if(it == meshes.end())
        return nullptr;
    return &(it->second);
}

// TODO: refactor to take span?
void VulkanRenderer::drawObjects(VkCommandBuffer cmd, RenderObject* first, int count)
{
    float framed = (frameNumber / 120.0f);
    sceneParameters.ambientColor = {sin(framed), 0, cos(framed), 1};
    char* sceneData; // char so can increment in single bytes
    vmaMapMemory(allocator, sceneParameterBuffer.allocation, (void**)&sceneData);
    int frameIndex = frameNumber % FRAMES_IN_FLIGHT;
    sceneData += padUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;
    memcpy(sceneData, &sceneParameters, sizeof(GPUSceneData));
    vmaUnmapMemory(allocator, sceneParameterBuffer.allocation);

    glm::vec3 camPos{0.f, -6.f, -10.f};
    // todo: replace with lookat
    glm::mat4 view = glm::translate(camPos);
    glm::mat4 projection =
        glm::perspective(glm::radians(70.0f), windowExtent.width / float(windowExtent.height), 0.1f, 200.0f);
    projection[1][1] *= -1;

    GPUCameraData camData;
    camData.proj = projection;
    camData.view = view;
    camData.projView = projection * view;

    // TODO: persistent binding
    void* data;
    vmaMapMemory(allocator, getCurrentFrameData().cameraBuffer.allocation, &data);
    memcpy(data, &camData, sizeof(GPUCameraData));
    vmaUnmapMemory(allocator, getCurrentFrameData().cameraBuffer.allocation);

    void* objectData;
    vmaMapMemory(allocator, getCurrentFrameData().objectBuffer.allocation, &objectData);
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    for(int i = 0; i < count; i++)
    {
        const RenderObject& object = first[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }
    vmaUnmapMemory(allocator, getCurrentFrameData().objectBuffer.allocation);

    Mesh* lastMesh = nullptr;
    Material* lastMaterial = nullptr;

    for(int i = 0; i < count; i++)
    {
        RenderObject& object = first[i];

        if(object.material != lastMaterial)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
            lastMaterial = object.material;

            uint32_t uniformOffset = padUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;
            // Also bind necessary descriptor sets
            // todo: global descriptor set shouldnt need to be bound for every material!
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                object.material->pipelineLayout,
                0,
                1,
                &getCurrentFrameData().globalDescriptor,
                1,
                &uniformOffset);

            // object data descriptor
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                object.material->pipelineLayout,
                1,
                1,
                &getCurrentFrameData().objectDescriptor,
                0,
                nullptr);
        }

        MeshPushConstants constants;
        constants.transformMatrix = object.transformMatrix;

        vkCmdPushConstants(
            cmd,
            object.material->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(MeshPushConstants),
            &constants);

        if(object.mesh != lastMesh)
        {
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->vertexBuffer.buffer, &offset);
            lastMesh = object.mesh;
        }

        vkCmdDraw(cmd, object.mesh->vertices.size(), 1, 0, i);
    }
}

AllocatedBuffer
VulkanRenderer::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferCrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,

        .size = allocSize,
        .usage = usage,
    };

    VmaAllocationCreateInfo vmaAllocCrInfo{
        .usage = memoryUsage,
    };

    AllocatedBuffer newBuffer;

    assertVkResult(vmaCreateBuffer(
        allocator, &bufferCrInfo, &vmaAllocCrInfo, &newBuffer.buffer, &newBuffer.allocation, nullptr));

    return newBuffer;
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