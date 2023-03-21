#include "VulkanRenderer.hpp"
#include "../Engine/Engine.hpp"
#include "Graphics/Mesh.hpp"
#include "Graphics/RenderObject.hpp"
#include "Graphics/Texture.hpp"
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
#include <iostream>
#include <stdexcept>
#include <string>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/trigonometric.hpp>
#include <vulkan/vulkan_core.h>

void VulkanRenderer::init()
{
    // Init window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(windowExtent.width, windowExtent.height, "Vulkan Test", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    *Engine::ptr->getMainWindow() = window;

    initVulkan();

    initSwapchain();

    initCommands();

    initDefaultRenderpass();

    initFramebuffers();

    initSyncStructures();

    initDescriptors();

    initPipelines();

    initImGui();

    loadMeshes();

    loadImages();

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
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
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
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 10},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 10},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
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
    sceneParameterBuffer = createBuffer(
        sceneParamBufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    for(int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        frames[i].cameraBuffer = createBuffer(
            sizeof(GPUCameraData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        const int MAX_OBJECTS = 10000;
        frames[i].objectBuffer = createBuffer(
            sizeof(GPUObjectData) * MAX_OBJECTS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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
    VkShaderModule texturedFragShader;
    if(!loadShaderModule("../shaders/textured_lit.frag.spirv", &texturedFragShader))
    {
        std::cout << "Error when building the textured fragment shader module" << std::endl;
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

    VkPipelineLayout texturedPipelineLayout;
    VkDescriptorSetLayout texturedSetLayouts[] = {globalSetLayout, objectSetLayout, singleTextureSetLayout};
    VkPipelineLayoutCreateInfo texturedPipelineLayoutCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .setLayoutCount = 3,
        .pSetLayouts = &texturedSetLayouts[0],
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };
    assertVkResult(
        vkCreatePipelineLayout(device, &texturedPipelineLayoutCrInfo, nullptr, &texturedPipelineLayout));

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

    VulkanPipeline texturedPipelineWrapper{VulkanPipeline::CreateInfo{
        .shaderStages =
            {{.stage = VK_SHADER_STAGE_VERTEX_BIT, .module = meshTriVertShader},
             {.stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = texturedFragShader}},
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
        .pipelineLayout = texturedPipelineLayout,
    }};
    // TODO: add a nice wrapper for this to pipeline constructor!
    texturedPipelineWrapper.vertexInputStateCrInfo.vertexBindingDescriptionCount =
        vertexDescription.bindings.size();
    texturedPipelineWrapper.vertexInputStateCrInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    texturedPipelineWrapper.vertexInputStateCrInfo.vertexAttributeDescriptionCount =
        vertexDescription.attributes.size();
    texturedPipelineWrapper.vertexInputStateCrInfo.pVertexAttributeDescriptions =
        vertexDescription.attributes.data();

    VkPipeline texturedPipeline = texturedPipelineWrapper.createPipeline(device, renderPass);

    createMaterial(texturedPipeline, texturedPipelineLayout, "texturedMesh");

    // Destroy these here already. (dont like though since VulkanPipeline object still exists and references
    // these!!, so could cause trouble in future when Pipeline needs to get recreated or something)
    vkDestroyShaderModule(device, meshTriVertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
    vkDestroyShaderModule(device, texturedFragShader, nullptr);

    deleteQueue.pushBack(
        [=]()
        {
            vkDestroyPipeline(device, meshPipeline, nullptr);
            vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);
            vkDestroyPipeline(device, texturedPipeline, nullptr);
            // vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);
        });
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
    ImGui_ImplGlfw_InitForVulkan(window, true);
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

    ImGui_ImplVulkan_Init(&initInfo, renderPass);

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
    glfwSetTime(0.0);
    Engine::ptr->getInputManager()->resetTime();

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        Engine::ptr->getInputManager()->update();
        if(!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
        {
            Engine::ptr->getCamera()->update();
        }
        // Not sure about the order of UI & engine code

        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Render();

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

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), curFrameData.mainCommandBuffer);

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

    Mesh lostEmpire{};
    lostEmpire.loadFromObj(ASSETS_PATH "/vkguide/lost_empire.obj");
    uploadMesh(lostEmpire);
    meshes["empire"] = lostEmpire;
}

void VulkanRenderer::uploadMesh(Mesh& mesh)
{
    const size_t bufferSize = mesh.vertices.size() * sizeof(Vertex);
    // allocate staging buffer
    VkBufferCreateInfo stagingBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    VmaAllocationCreateInfo vmaallocCrInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    AllocatedBuffer stagingBuffer;
    assertVkResult(vmaCreateBuffer(
        allocator,
        &stagingBufferInfo,
        &vmaallocCrInfo,
        &stagingBuffer.buffer,
        &stagingBuffer.allocation,
        &stagingBuffer.allocInfo));

    void* data = nullptr;
    vmaMapMemory(allocator, stagingBuffer.allocation, &data);
    memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator, stagingBuffer.allocation);

    // GPU side buffer
    VkBufferCreateInfo vertexBufferCrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };
    vmaallocCrInfo = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    assertVkResult(vmaCreateBuffer(
        allocator,
        &vertexBufferCrInfo,
        &vmaallocCrInfo,
        &mesh.vertexBuffer.buffer,
        &mesh.vertexBuffer.allocation,
        nullptr));

    immediateSubmit(
        [=](VkCommandBuffer cmd)
        {
            VkBufferCopy copy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = bufferSize,
            };
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1, &copy);
        });

    deleteQueue.pushBack([=]()
                         { vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation); });

    // since immediateSubmit also waits until the commands have executed, we can safely delete the staging buffer
    // immediately here
    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

void VulkanRenderer::loadImages()
{
    Texture lostEmpire;
    loadImageFromFile(*this, ASSETS_PATH "/vkguide/lost_empire-RGBA.png", lostEmpire.image);

    VkImageViewCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = lostEmpire.image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCreateImageView(device, &imageInfo, nullptr, &lostEmpire.imageView);

    loadedTextures["empire_diffuse"] = lostEmpire;

    deleteQueue.pushBack([=]() { vkDestroyImageView(device, lostEmpire.imageView, nullptr); });
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

    RenderObject map;
    map.mesh = getMesh("empire");
    map.material = getMaterial("texturedMesh");
    map.transformMatrix = glm::translate(glm::vec3{5.0f, -10.0f, 0.0f});
    renderables.push_back(map);

    // Descriptors using the loaded textures

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

    Material* texturedMat = getMaterial("texturedMesh");

    VkDescriptorSetAllocateInfo descSetAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &singleTextureSetLayout,
    };
    vkAllocateDescriptorSets(device, &descSetAllocInfo, &texturedMat->textureSet);

    VkDescriptorImageInfo imageBufferInfo{
        .sampler = blockySampler,
        .imageView = loadedTextures["empire_diffuse"].imageView,
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
    char* sceneData = reinterpret_cast<char*>(
        sceneParameterBuffer.allocInfo.pMappedData); // char so can increment in single bytes
    // dont need to map, was requested to be persistently mapped
    int frameIndex = frameNumber % FRAMES_IN_FLIGHT;
    sceneData += padUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;
    memcpy(sceneData, &sceneParameters, sizeof(GPUSceneData));
    // dont need to unmap, was requested to be coherent

    Camera* mainCamera = Engine::ptr->getCamera();
    // todo: resize GPUCameraData to use all matrices Camera stores and then simply memcpy from
    // mainCamera.getMatrices directly
    GPUCameraData camData;
    camData.proj = mainCamera->getProj();
    camData.view = mainCamera->getView();
    camData.projView = mainCamera->getProjView();

    void* data = getCurrentFrameData().cameraBuffer.allocInfo.pMappedData;
    memcpy(data, &camData, sizeof(GPUCameraData));

    void* objectData = getCurrentFrameData().objectBuffer.allocInfo.pMappedData;
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    for(int i = 0; i < count; i++)
    {
        const RenderObject& object = first[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }

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

            if(object.material->textureSet != VK_NULL_HANDLE)
            {
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    object.material->pipelineLayout,
                    2,
                    1,
                    &object.material->textureSet,
                    0,
                    nullptr);
            }
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

AllocatedBuffer VulkanRenderer::createBuffer(
    size_t allocSize,
    VkBufferUsageFlags usage,
    VmaAllocationCreateFlags flags,
    VkMemoryPropertyFlags requiredFlags)
{
    VkBufferCreateInfo bufferCrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,

        .size = allocSize,
        .usage = usage,
    };

    VmaAllocationCreateInfo vmaAllocCrInfo{
        .flags = flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = requiredFlags,
    };

    AllocatedBuffer newBuffer;

    assertVkResult(vmaCreateBuffer(
        allocator,
        &bufferCrInfo,
        &vmaAllocCrInfo,
        &newBuffer.buffer,
        &newBuffer.allocation,
        &newBuffer.allocInfo));

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