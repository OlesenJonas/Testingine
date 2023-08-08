#include "Editor.hpp"
#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Mesh/Cube.hpp>
#include <Engine/Graphics/Texture/TextureToVulkan.hpp>
#include <Engine/Scene/DefaultComponents.hpp>
#include <Engine/Scene/Scene.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <format>
#include <fstream>

Editor::Editor()
    : Application(Application::CreateInfo{
          .name = "Testingine Editor",
          .windowHints = {{GLFW_MAXIMIZED, GLFW_TRUE}},
      }),
      sceneRoot(ecs.createEntity())
{
    renderer.defaultDepthFormat = toVkFormat(depthFormat);

    inputManager.init(mainWindow.glfwWindow);

    glfwSetWindowUserPointer(mainWindow.glfwWindow, this);
    // TODO: FIX: THIS OVERRIDES IMGUIS CALLBACKS!
    inputManager.setupCallbacks(
        mainWindow.glfwWindow, keyCallback, mouseButtonCallback, scrollCallback, resizeCallback);

    ecs.registerComponent<Transform>();
    ecs.registerComponent<Hierarchy>();
    ecs.registerComponent<RenderInfo>();

    sceneRoot.addComponent<Transform>();
    sceneRoot.addComponent<Hierarchy>();

    // rendering internals -------------------------

    for(int i = 0; i < ArraySize(perFrameData); i++)
    {
        perFrameData[i].renderPassDataBuffer = resourceManager.createBuffer(Buffer::CreateInfo{
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
        perFrameData[i].objectBuffer = resourceManager.createBuffer(Buffer::CreateInfo{
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

    depthTexture = resourceManager.createTexture(Texture::CreateInfo{
        .debugName = "Depth Texture",
        .format = depthFormat,
        .allStates = ResourceState::DepthStencilTarget,
        .initialState = ResourceState::Undefined,
        .size = {renderer.getSwapchainWidth(), renderer.getSwapchainHeight()},
    });

    // Create default Samplers

    struct DefaultSamplerInfo
    {
        int index;
        std::string_view name;
        Sampler::Info info;
    };

    DefaultSamplerInfo defaultInfos[] = {
        DefaultSamplerInfo{
            .name = "DEFAULT_SAMPLER_LINEAR_REPEAT",
            .info =
                {
                    .magFilter = Sampler::Filter::Linear,
                    .minFilter = Sampler::Filter::Linear,
                    .mipMapFilter = Sampler::Filter::Linear,
                    .addressModeU = Sampler::AddressMode::Repeat,
                    .addressModeV = Sampler::AddressMode::Repeat,
                    .addressModeW = Sampler::AddressMode::Repeat,
                }},
        DefaultSamplerInfo{
            .name = "DEFAULT_SAMPLER_LINEAR_CLAMP",
            .info =
                {
                    .magFilter = Sampler::Filter::Linear,
                    .minFilter = Sampler::Filter::Linear,
                    .mipMapFilter = Sampler::Filter::Linear,
                    .addressModeU = Sampler::AddressMode::ClampEdge,
                    .addressModeV = Sampler::AddressMode::ClampEdge,
                    .addressModeW = Sampler::AddressMode::ClampEdge,
                }},
        DefaultSamplerInfo{
            .name = "DEFAULT_SAMPLER_NEAREST_CLAMP",
            .info =
                {
                    .magFilter = Sampler::Filter::Nearest,
                    .minFilter = Sampler::Filter::Nearest,
                    .mipMapFilter = Sampler::Filter::Nearest,
                    .addressModeU = Sampler::AddressMode::ClampEdge,
                    .addressModeV = Sampler::AddressMode::ClampEdge,
                    .addressModeW = Sampler::AddressMode::ClampEdge,
                }},
    };
    for(auto& info : defaultInfos)
    {
        auto samplerHandle = resourceManager.createSampler(Sampler::Info{info.info});
        info.index = samplerHandle.getIndex();
    }

    // TODO: this only really has to happen once post build, could make own executable that uses shared header or
    //       smth!
    //          also think about how this works once .spirvs are distributed (outside of source tree) etc...
    std::ofstream defaultSamplerDefines(
        SHADERS_PATH "/Bindless/DefaultSamplers.hlsl", std::ofstream::out | std::ofstream::trunc);
    for(auto& info : defaultInfos)
    {
        defaultSamplerDefines << std::format("#define {} {}\n", info.name, info.index);
    }
    defaultSamplerDefines.close();

    resourceManager.createMaterial(
        {
            .vertexShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasic.vert"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasic.frag"},
        },
        "PBRBasic");
    assert(resourceManager.get(resourceManager.getMaterial("PBRBasic")) != nullptr);

    resourceManager.createMesh(Cube::positions, Cube::attributes, Cube::indices, "DefaultCube");
    assert(resourceManager.get(resourceManager.getMesh("DefaultCube")) != nullptr);

    mainCamera =
        Camera{static_cast<float>(mainWindow.width) / static_cast<float>(mainWindow.height), 0.1f, 1000.0f};

    glfwSetTime(0.0);
    inputManager.resetTime();

    // ------------------------------------------------------

    // single startup frame, so device is in correct state for rendering commands to be inserted between init() and
    // run()
    frameNumber++;
    renderer.startNextFrame();

    VkCommandBuffer mainCmdBuffer = renderer.beginCommandBuffer();
    renderer.insertSwapchainImageBarrier(
        mainCmdBuffer, ResourceState::OldSwapchainImage, ResourceState::PresentSrc);

    // Scene and other test stuff loading -------------------------------------------

    // ------------------------------------------------------------------------------

    renderer.endCommandBuffer(mainCmdBuffer);
    renderer.submitCommandBuffers({mainCmdBuffer});
    // Needed so swapchain "progresses" (see vulkan validation message) TODO: fix
    renderer.presentSwapchain();

    // just to be safe, wait for all commands to be done here
    renderer.waitForWorkFinished();
}

Editor::~Editor()
{
    // TODO: need a fancy way of ensuring that this is always called in applications
    //       Cant use base class destructor since that will only be called *after*wards
    renderer.waitForWorkFinished();
}

void Editor::run()
{
    while(_isRunning)
    {
        update();
    }
}

void Editor::update()
{
    _isRunning &= !glfwWindowShouldClose(mainWindow.glfwWindow);
    if(!_isRunning)
        return;

    frameNumber++;

    glfwPollEvents();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    inputManager.update(mainWindow.glfwWindow);
    if(!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
    {
        mainCamera.update(mainWindow.glfwWindow, &inputManager);
    }
    // Not sure about the order of UI & engine code

    ImGui::ShowDemoWindow();
    ImGui::Render();

    // --------------- Rendering code -----------------------------

    // TODO: ASSERTS AND/OR WARNINGS TO MAKE SURE ALL NECESSARY COMMANDS CALLED, AND IN CORRECT ORDER
    //          (ie error out when command buffer begun before next frame started)
    renderer.startNextFrame();

    VkCommandBuffer mainCmdBuffer = renderer.beginCommandBuffer();

    renderer.submitBarriers(
        mainCmdBuffer,
        {
            Barrier::from(Barrier::Image{
                .texture = depthTexture,
                .stateBefore = ResourceState::DepthStencilTarget,
                .stateAfter = ResourceState::DepthStencilTarget,
                .allowDiscardOriginal = true,
            }),
        });
    renderer.insertSwapchainImageBarrier(
        mainCmdBuffer, ResourceState::OldSwapchainImage, ResourceState::Rendertarget);

    renderer.beginRendering(
        mainCmdBuffer,
        {RenderTarget{.texture = RenderTarget::SwapchainImage{}, .loadOp = RenderTarget::LoadOp::Clear}},
        RenderTarget{.texture = depthTexture, .loadOp = RenderTarget::LoadOp::Clear});

    // Render scene ------------------------------

    struct RenderObject
    {
        Handle<Mesh> mesh;
        Handle<MaterialInstance> materialInstance;
        glm::mat4 transformMatrix;
    };
    std::vector<RenderObject> renderables;

    ecs.forEach<RenderInfo, Transform>(
        [&](RenderInfo* renderinfos, Transform* transforms, uint32_t count)
        {
            for(int i = 0; i < count; i++)
            {
                const RenderInfo& rinfo = renderinfos[i];
                const Transform& transform = transforms[i];
                renderables.emplace_back(rinfo.mesh, rinfo.materialInstance, transform.localToWorld);
            }
        });

    RenderPassData renderPassData;
    renderPassData.proj = mainCamera.getProj();
    renderPassData.view = mainCamera.getView();
    renderPassData.projView = mainCamera.getProjView();
    renderPassData.cameraPositionWS = mainCamera.getPosition();

    Buffer* renderPassDataBuffer = resourceManager.get(getCurrentFrameData().renderPassDataBuffer);

    void* data = renderPassDataBuffer->allocInfo.pMappedData;
    memcpy(data, &renderPassData, sizeof(renderPassData));

    Buffer* objectBuffer = resourceManager.get(getCurrentFrameData().objectBuffer);

    void* objectData = objectBuffer->allocInfo.pMappedData;
    // not sure how good assigning single GPUObjectDatas is (vs CPU buffer and then one memcpy)
    auto* objectSSBO = (GPUObjectData*)objectData;
    for(int i = 0; i < renderables.size(); i++)
    {
        const RenderObject& object = renderables[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }

    //---

    BindlessIndices pushConstants;
    pushConstants.RenderInfoBuffer = renderPassDataBuffer->resourceIndex;
    pushConstants.transformBuffer = objectBuffer->resourceIndex;

    Handle<Mesh> lastMesh = Handle<Mesh>::Invalid();
    Handle<Material> lastMaterial = Handle<Material>::Invalid();
    Handle<MaterialInstance> lastMaterialInstance = Handle<MaterialInstance>::Invalid();
    uint32_t indexCount = 0;

    for(int i = 0; i < renderables.size(); i++)
    {
        RenderObject& object = renderables[i];

        Handle<Mesh> objectMesh = object.mesh;
        Handle<MaterialInstance> objectMaterialInstance = object.materialInstance;

        if(objectMaterialInstance != lastMaterialInstance)
        {
            MaterialInstance* newMatInst = resourceManager.get(objectMaterialInstance);
            if(newMatInst->parentMaterial != lastMaterial)
            {
                Material* newMat = resourceManager.get(newMatInst->parentMaterial);
                renderer.setPipelineState(mainCmdBuffer, newMatInst->parentMaterial);
                Buffer* materialParamsBuffer = resourceManager.get(newMat->parameters.getGPUBuffer());
                if(materialParamsBuffer != nullptr)
                {
                    pushConstants.materialParamsBuffer = materialParamsBuffer->resourceIndex;
                }
                lastMaterial = newMatInst->parentMaterial;
            }

            Buffer* materialInstanceParamsBuffer = resourceManager.get(newMatInst->parameters.getGPUBuffer());
            if(materialInstanceParamsBuffer != nullptr)
            {
                pushConstants.materialInstanceParamsBuffer = materialInstanceParamsBuffer->resourceIndex;
            }
        }

        pushConstants.transformIndex = i;
        renderer.pushConstants(mainCmdBuffer, sizeof(BindlessIndices), &pushConstants);

        if(objectMesh != lastMesh)
        {
            Mesh* newMesh = resourceManager.get(objectMesh);
            indexCount = newMesh->indexCount;
            Buffer* indexBuffer = resourceManager.get(newMesh->indexBuffer);
            Buffer* positionBuffer = resourceManager.get(newMesh->positionBuffer);
            Buffer* attributeBuffer = resourceManager.get(newMesh->attributeBuffer);
            renderer.bindIndexBuffer(mainCmdBuffer, newMesh->indexBuffer);
            renderer.bindVertexBuffers(
                mainCmdBuffer, 0, 2, {newMesh->positionBuffer, newMesh->attributeBuffer}, {0, 0});
            lastMesh = objectMesh;
        }

        renderer.drawIndexed(mainCmdBuffer, indexCount, 1, 0, 0, 0);
    }

    renderer.endRendering(mainCmdBuffer);

    // Draw UI ---------

    renderer.beginRendering(
        mainCmdBuffer,
        {RenderTarget{.texture = RenderTarget::SwapchainImage{}, .loadOp = RenderTarget::LoadOp::Load}},
        RenderTarget{.texture = Handle<Texture>::Null()});
    renderer.drawImGui(mainCmdBuffer);
    renderer.endRendering(mainCmdBuffer);

    renderer.insertSwapchainImageBarrier(mainCmdBuffer, ResourceState::Rendertarget, ResourceState::PresentSrc);

    renderer.endCommandBuffer(mainCmdBuffer);

    renderer.submitCommandBuffers({mainCmdBuffer});

    renderer.presentSwapchain();

    // ------------------------------
}