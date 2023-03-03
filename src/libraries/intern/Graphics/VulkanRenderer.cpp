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

    initPipelines();

    loadMeshes();

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

    assertVkResult(vkCreateCommandPool(device, &commandPoolCrInfo, nullptr, &commandPool));

    VkCommandBufferAllocateInfo cmdBuffAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,

        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    assertVkResult(vkAllocateCommandBuffers(device, &cmdBuffAllocInfo, &mainCommandBuffer));

    deleteQueue.pushBack([=]() { vkDestroyCommandPool(device, commandPool, nullptr); });
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

    assertVkResult(vkCreateFence(device, &fenceCreateInfo, nullptr, &renderFence));

    deleteQueue.pushBack([=]() { vkDestroyFence(device, renderFence, nullptr); });

    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };

    assertVkResult(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentSemaphore));
    assertVkResult(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSemaphore));

    deleteQueue.pushBack(
        [=]()
        {
            vkDestroySemaphore(device, presentSemaphore, nullptr);
            vkDestroySemaphore(device, renderSemaphore, nullptr);
        });
}

void VulkanRenderer::initPipelines()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    assertVkResult(vkCreatePipelineLayout(device, &pipelineLayoutCrInfo, nullptr, &trianglePipelineLayout));

    VkShaderModule triangleVertShader;
    if(!loadShaderModule("../shaders/colored_tri.vert.spirv", &triangleVertShader))
    {
        std::cout << "Error when building the triangle vertex shader module" << std::endl;
    }
    VkShaderModule triangleFragShader;
    if(!loadShaderModule("../shaders/colored_tri.frag.spirv", &triangleFragShader))
    {
        std::cout << "Error when building the triangle fragment shader module" << std::endl;
    }

    VulkanPipeline trianglePipelineWrapper{VulkanPipeline::CreateInfo{
        .shaderStages =
            {{.stage = VK_SHADER_STAGE_VERTEX_BIT, .module = triangleVertShader},
             {.stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = triangleFragShader}},
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
        .pipelineLayout = trianglePipelineLayout,
    }};

    trianglePipeline = trianglePipelineWrapper.createPipeline(device, renderPass);

    // Mesh Pipeline
    VkShaderModule meshTriVertShader;
    if(!loadShaderModule("../shaders/tri_mesh.vert.spirv", &meshTriVertShader))
    {
        std::cout << "Error when building the triangle vertex shader module" << std::endl;
    }

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(MeshPushConstants),
    };
    VkPipelineLayoutCreateInfo meshPipelineLayoutCrInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };
    assertVkResult(vkCreatePipelineLayout(device, &meshPipelineLayoutCrInfo, nullptr, &meshPipelineLayout));

    VertexInputDescription vertexDescription = Vertex::getVertexDescription();

    VulkanPipeline meshPipelineWrapper{VulkanPipeline::CreateInfo{
        .shaderStages =
            {{.stage = VK_SHADER_STAGE_VERTEX_BIT, .module = meshTriVertShader},
             {.stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = triangleFragShader}},
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

    meshPipeline = meshPipelineWrapper.createPipeline(device, renderPass);

    // Destroy these here already. (dont like though since VulkanPipeline object still exists and references
    // these!!, so could cause trouble in future when Pipeline needs to get recreated or something)
    vkDestroyShaderModule(device, triangleFragShader, nullptr);
    vkDestroyShaderModule(device, triangleVertShader, nullptr);
    vkDestroyShaderModule(device, meshTriVertShader, nullptr);

    deleteQueue.pushBack(
        [=]()
        {
            vkDestroyPipeline(device, trianglePipeline, nullptr);
            vkDestroyPipeline(device, meshPipeline, nullptr);
            vkDestroyPipelineLayout(device, trianglePipelineLayout, nullptr);
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
    assertVkResult(vkWaitForFences(device, 1, &renderFence, true, UINT64_MAX));
    assertVkResult(vkResetFences(device, 1, &renderFence));

    uint32_t swapchainImageIndex;
    assertVkResult(
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentSemaphore, nullptr, &swapchainImageIndex));

    assertVkResult(vkResetCommandBuffer(mainCommandBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,

        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    assertVkResult(vkBeginCommandBuffer(mainCommandBuffer, &cmdBeginInfo));

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

    vkCmdBeginRenderPass(mainCommandBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);

    VkDeviceSize offset = 0;
    // vkCmdBindVertexBuffers(mainCommandBuffer, 0, 1, &triangleMesh.vertexBuffer.buffer, &offset);
    vkCmdBindVertexBuffers(mainCommandBuffer, 0, 1, &monkeyMesh.vertexBuffer.buffer, &offset);

    glm::vec3 camPos{0.f, 0.f, -2.0f};
    // glm::mat4 view = glm::translate(glm::mat4(1.0f), camPos);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});
    glm::mat4 projection =
        glm::perspective(glm::radians(70.0f), float(windowExtent.width) / windowExtent.height, 0.1f, 200.0f);
    projection[1][1] *= -1;
    glm::mat4 model = glm::rotate(glm::radians(frameNumber * 0.114f), glm::vec3{0, 1, 0});
    glm::mat4 transformMatrix = projection * view * model;

    MeshPushConstants constants;
    constants.transformMatrix = transformMatrix;

    vkCmdPushConstants(
        mainCommandBuffer,
        meshPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(MeshPushConstants),
        &constants);

    // vkCmdDraw(mainCommandBuffer, triangleMesh.vertices.size(), 1, 0, 0);
    vkCmdDraw(mainCommandBuffer, monkeyMesh.vertices.size(), 1, 0, 0);

    vkCmdEndRenderPass(mainCommandBuffer);

    assertVkResult(vkEndCommandBuffer(mainCommandBuffer));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &presentSemaphore,
        .pWaitDstStageMask = &waitStage,

        .commandBufferCount = 1,
        .pCommandBuffers = &mainCommandBuffer,

        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderSemaphore,
    };

    assertVkResult(vkQueueSubmit(graphicsQueue, 1, &submitInfo, renderFence));

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderSemaphore,

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
    monkeyMesh.loadFromObj(ASSETS_PATH "/vkguide/monkey_smooth.obj");

    uploadMesh(monkeyMesh);
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