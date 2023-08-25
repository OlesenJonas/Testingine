#include "Editor.hpp"
#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Barrier/Barrier.hpp>
#include <Engine/Graphics/Mesh/Cube.hpp>
#include <Engine/Misc/Math.hpp>
#include <Engine/Scene/DefaultComponents.hpp>
#include <Engine/Scene/Scene.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <format>
#include <fstream>
#include <future>

Editor::Editor()
    : Application(Application::CreateInfo{
          .name = "Testingine Editor",
          .windowHints = {{GLFW_MAXIMIZED, GLFW_TRUE}},
      }),
      sceneRoot(ecs.createEntity())
{
    gfxDevice.startInitializationWork();

    gfxDevice.defaultDepthFormat = toVkFormat(depthFormat);

    inputManager.init(mainWindow.glfwWindow);
    glfwSetWindowUserPointer(mainWindow.glfwWindow, this);
    inputManager.setupCallbacks(
        mainWindow.glfwWindow, keyCallback, mouseButtonCallback, scrollCallback, resizeCallback);

    // reserve index 0 for main thread
    auto index = threadPool.getThreadPoolThreadIndex();
    assert(index == 0);
    ThreadPool::nameCurrentThread("Main Thread");
    threadPool.start(4);

    ecs.registerComponent<Transform>();
    ecs.registerComponent<Hierarchy>();
    ecs.registerComponent<RenderInfo>();

    sceneRoot.addComponent<Transform>();
    sceneRoot.addComponent<Hierarchy>();

    // rendering internals -------------------------

    for(int i = 0; i < ArraySize(perFrameData); i++)
    {
        perFrameData[i].renderPassDataBuffer = resourceManager.createBuffer(Buffer::CreateInfo{
            .debugName = ("RenderPassDataBuffer" + std::to_string(i)),
            .size = sizeof(RenderPassData),
            .memoryType = Buffer::MemoryType::GPU_BUT_CPU_VISIBLE,
            .allStates = ResourceState::UniformBuffer,
            .initialState = ResourceState::UniformBuffer,
        });

        const int MAX_OBJECTS = 10000;
        perFrameData[i].objectBuffer = resourceManager.createBuffer(Buffer::CreateInfo{
            .debugName = ("ObjectBuffer" + std::to_string(i)),
            .size = sizeof(GPUObjectData) * MAX_OBJECTS,
            .memoryType = Buffer::MemoryType::GPU_BUT_CPU_VISIBLE,
            .allStates = ResourceState::Storage,
            .initialState = ResourceState::Storage,
        });
    }

    depthTexture = resourceManager.createTexture(Texture::CreateInfo{
        .debugName = "Depth Texture",
        .format = depthFormat,
        .allStates = ResourceState::DepthStencilTarget,
        .initialState = ResourceState::Undefined,
        .size = {gfxDevice.getSwapchainWidth(), gfxDevice.getSwapchainHeight()},
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

    resourceManager.createMaterial({
        .vertexShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasic.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasic.frag"},
        .debugName = "PBRBasic",
    });
    assert(resourceManager.get(resourceManager.getMaterial("PBRBasic")) != nullptr);

    resourceManager.createMesh(Cube::positions, Cube::attributes, Cube::indices, "DefaultCube");
    assert(resourceManager.get(resourceManager.getMesh("DefaultCube")) != nullptr);

    mainCamera =
        Camera{static_cast<float>(mainWindow.width) / static_cast<float>(mainWindow.height), 0.1f, 1000.0f};

    glfwSetTime(0.0);
    inputManager.resetTime();

    // Scene and other test stuff loading -------------------------------------------
    VkCommandBuffer mainCmdBuffer = gfxDevice.beginCommandBuffer();

    // Disable validation error breakpoints during init, synch errors arent correct
    gfxDevice.disableValidationErrorBreakpoint();

    Scene::load("C:/Users/jonas/Documents/Models/DamagedHelmet/DamagedHelmet.gltf", &ecs, sceneRoot);

    Handle<ComputeShader> debugMipFillShaderH = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/Misc/debugMipFill.comp"}, "debugMipFill");
    ComputeShader* debugMipFillShader = resourceManager.get(debugMipFillShaderH);

    auto mipTestTexH = resourceManager.createTexture({
        .debugName = "mipTest",
        .type = Texture::Type::t2D,
        .format = Texture::Format::r8g8b8a8_unorm,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {128, 128},
        .mipLevels = Texture::MipLevels::All,
    });
    {
        gfxDevice.startDebugRegion(mainCmdBuffer, "Mip test tex filling");
        gfxDevice.setComputePipelineState(mainCmdBuffer, resourceManager.get(debugMipFillShaderH)->pipeline);

        struct DebugMipFillPushConstants
        {
            uint32_t targetIndex;
            uint32_t level;
        };
        DebugMipFillPushConstants constants{
            .targetIndex = 0xFFFFFFFF,
            .level = 0,
        };

        uint32_t mipCount = resourceManager.get<Texture::Descriptor>(mipTestTexH)->mipLevels;
        for(uint32_t mip = 0; mip < mipCount; mip++)
        {
            Handle<TextureView> mipView = gfxDevice.createTextureView(TextureView::CreateInfo{
                .parent = mipTestTexH,
                .allStates = ResourceState::StorageCompute,
                .baseMipLevel = mip,
            });

            uint32_t mipSize = glm::max(128u >> mip, 1u);

            constants = {
                .targetIndex = gfxDevice.get(mipView)->resourceIndex,
                .level = mip,
            };

            gfxDevice.pushConstants(mainCmdBuffer, sizeof(DebugMipFillPushConstants), &constants);

            // TODO: dont hardcode sizes! retrieve programmatically (workrgoup size form spirv)
            gfxDevice.dispatchCompute(mainCmdBuffer, UintDivAndCeil(mipSize, 8), UintDivAndCeil(mipSize, 8), 6);

            gfxDevice.destroy(mipView);
        }

        // transfer dst texture from general to shader read only layout
        gfxDevice.insertBarriers(
            mainCmdBuffer,
            {
                Barrier::FromImage{
                    .texture = mipTestTexH,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });
        gfxDevice.endDebugRegion(mainCmdBuffer);
    }

    Handle<Mesh> triangleMesh;
    {
        std::vector<glm::vec3> triangleVertexPositions;
        std::vector<Mesh::VertexAttributes> triangleVertexAttributes;
        triangleVertexPositions.resize(3);
        triangleVertexAttributes.resize(3);

        triangleVertexPositions[0] = {1.f, 1.f, 0.0f};
        triangleVertexPositions[1] = {-1.f, 1.f, 0.0f};
        triangleVertexPositions[2] = {0.f, -1.f, 0.0f};

        triangleVertexAttributes[0].uv = {1.0f, 0.0f};
        triangleVertexAttributes[1].uv = {0.0f, 0.0f};
        triangleVertexAttributes[2].uv = {0.5f, 1.0f};

        triangleMesh =
            resourceManager.createMesh(triangleVertexPositions, triangleVertexAttributes, {}, "triangle");
    }

    auto unlitTexturedMaterialH = resourceManager.createMaterial({
        .vertexShader = {.sourcePath = SHADERS_PATH "/Unlit/TexturedUnlit.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/Unlit/TexturedUnlit.frag"},
        .debugName = "texturedUnlit",
    });

    auto unlitTexturedMaterial = resourceManager.createMaterialInstance(unlitTexturedMaterialH);
    resourceManager.get(unlitTexturedMaterial)
        ->setResource("texture", *resourceManager.get<ResourceIndex>(mipTestTexH));

    auto newRenderable = ecs.createEntity();
    {
        auto* renderInfo = newRenderable.addComponent<RenderInfo>();
        renderInfo->mesh = triangleMesh;
        renderInfo->materialInstance = unlitTexturedMaterial;
        auto* transform = newRenderable.addComponent<Transform>();
        transform->position = glm::vec3{3.0f, 0.0f, 0.0f};
        transform->calculateLocalTransformMatrix();
        // dont like having to call this manually
        transform->localToWorld = transform->localTransform;
        /*
            maybe a wrapper around entt + default component creation like
            scene.addObject() that just conatins Transform+Hierarchy component (and default parenting
            settings) would be nice
        */
        assert(renderInfo->mesh.isValid());
        assert(renderInfo->materialInstance.isValid());
    }

    auto hdri = resourceManager.createTexture(Texture::LoadInfo{
        .path = ASSETS_PATH "/HDRIs/kloppenheim_04_2k.hdr",
        .fileDataIsLinear = true,
        .allStates = ResourceState::SampleSource,
        .initialState = ResourceState::SampleSource,
    });
    const int32_t hdriCubeRes = 512;
    auto hdriCube = resourceManager.createTexture(Texture::CreateInfo{
        // specifying non-default values only
        .debugName = "HdriCubemap",
        .type = Texture::Type::tCube,
        .format = resourceManager.get<Texture::Descriptor>(hdri)->format,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {.width = hdriCubeRes, .height = hdriCubeRes},
        .mipLevels = Texture::MipLevels::All,
    });
    Handle<ComputeShader> conversionShaderHandle = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/Skybox/equiToCube.hlsl"}, "equiToCubeCompute");
    ComputeShader* conversionShader = resourceManager.get(conversionShaderHandle);

    {
        gfxDevice.setComputePipelineState(mainCmdBuffer, resourceManager.get(conversionShaderHandle)->pipeline);

        struct ConversionPushConstants
        {
            uint32_t sourceIndex;
            uint32_t targetIndex;
        } constants = {
            .sourceIndex = *resourceManager.get<ResourceIndex>(hdri),
            // TODO: Not sure if writing into image view with all levels correctly writes into 0th level
            .targetIndex = *resourceManager.get<ResourceIndex>(hdriCube),
        };
        gfxDevice.pushConstants(mainCmdBuffer, sizeof(ConversionPushConstants), &constants);

        gfxDevice.dispatchCompute(mainCmdBuffer, hdriCubeRes / 8, hdriCubeRes / 8, 6);

        gfxDevice.insertBarriers(
            mainCmdBuffer,
            {
                Barrier::FromImage{
                    .texture = hdriCube,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });

        gfxDevice.fillMipLevels(mainCmdBuffer, hdriCube, ResourceState::SampleSource);
    }

    Handle<ComputeShader> calcIrradianceComp = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/Skybox/generateIrradiance.comp"}, "generateIrradiance");
    ComputeShader* calcIrradianceShader = resourceManager.get(calcIrradianceComp);

    uint32_t irradianceRes = 32;
    auto irradianceTexHandle = resourceManager.createTexture({
        .debugName = "hdriIrradiance",
        .type = Texture::Type::tCube,
        .format = Texture::Format::r16g16b16a16_float,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {irradianceRes, irradianceRes, 1},
    });
    {
        gfxDevice.setComputePipelineState(mainCmdBuffer, resourceManager.get(calcIrradianceComp)->pipeline);

        struct ConversionPushConstants
        {
            uint32_t sourceIndex;
            uint32_t targetIndex;
        };
        ConversionPushConstants constants{
            .sourceIndex = *resourceManager.get<ResourceIndex>(hdriCube),
            .targetIndex = *resourceManager.get<ResourceIndex>(irradianceTexHandle),
        };

        gfxDevice.pushConstants(mainCmdBuffer, sizeof(ConversionPushConstants), &constants);

        gfxDevice.dispatchCompute(mainCmdBuffer, irradianceRes / 8, irradianceRes / 8, 6);

        // transfer dst texture from general to shader read only layout
        gfxDevice.insertBarriers(
            mainCmdBuffer,
            {
                Barrier::FromImage{
                    .texture = irradianceTexHandle,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });
    }

    Handle<ComputeShader> prefilterEnvShaderHandle = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/Skybox/prefilter.comp"}, "prefilterEnvComp");
    ComputeShader* prefilterEnvShader = resourceManager.get(prefilterEnvShaderHandle);

    uint32_t prefilteredEnvMapBaseSize = 128;
    auto prefilteredEnvMap = resourceManager.createTexture({
        .debugName = "prefilteredEnvMap",
        .type = Texture::Type::tCube,
        .format = Texture::Format::r16g16b16a16_float,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {prefilteredEnvMapBaseSize, prefilteredEnvMapBaseSize},
        .mipLevels = 5,
    });
    {
        gfxDevice.setComputePipelineState(mainCmdBuffer, resourceManager.get(prefilterEnvShaderHandle)->pipeline);

        struct PrefilterPushConstants
        {
            uint32_t sourceIndex;
            uint32_t targetIndex;
            float roughness;
        };
        PrefilterPushConstants constants{
            .sourceIndex = 0xFFFFFFFF,
            .targetIndex = 0xFFFFFFFF,
            .roughness = 0.0,
        };

        uint32_t mipCount = resourceManager.get<Texture::Descriptor>(prefilteredEnvMap)->mipLevels;
        for(uint32_t mip = 0; mip < mipCount; mip++)
        {
            Handle<TextureView> mipView = gfxDevice.createTextureView(TextureView::CreateInfo{
                .parent = prefilteredEnvMap,
                .type = Texture::Type::tCube,
                .allStates = ResourceState::StorageCompute,
                .baseMipLevel = mip,
            });

            uint32_t mipSize = glm::max(prefilteredEnvMapBaseSize >> mip, 1u);

            constants = {
                .sourceIndex = *resourceManager.get<ResourceIndex>(hdriCube),
                .targetIndex = gfxDevice.get(mipView)->resourceIndex,
                .roughness = float(mip) / float(mipCount - 1),
            };

            gfxDevice.pushConstants(mainCmdBuffer, sizeof(PrefilterPushConstants), &constants);

            gfxDevice.dispatchCompute(mainCmdBuffer, UintDivAndCeil(mipSize, 8), UintDivAndCeil(mipSize, 8), 6);

            gfxDevice.destroy(mipView);
        }

        // transfer dst texture from general to shader read only layout
        gfxDevice.insertBarriers(
            mainCmdBuffer,
            {
                Barrier::FromImage{
                    .texture = prefilteredEnvMap,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });
    }

    Handle<ComputeShader> integrateBrdfShaderHandle = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/PBR/integrateBRDF.comp"}, "integrateBRDFComp");
    ComputeShader* integrateBRDFShader = resourceManager.get(integrateBrdfShaderHandle);
    uint32_t brdfIntegralSize = 512;
    auto brdfIntegralMap = resourceManager.createTexture({
        .debugName = "brdfIntegral",
        .type = Texture::Type::t2D,
        .format = Texture::Format::r16_g16_float,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {brdfIntegralSize, brdfIntegralSize},
        .mipLevels = 1,
    });
    {
        gfxDevice.setComputePipelineState(mainCmdBuffer, resourceManager.get(integrateBrdfShaderHandle)->pipeline);

        struct IntegratePushConstants
        {
            uint32_t outTexture;
        };
        IntegratePushConstants constants{*resourceManager.get<ResourceIndex>(brdfIntegralMap)};

        gfxDevice.pushConstants(mainCmdBuffer, sizeof(IntegratePushConstants), &constants);

        gfxDevice.dispatchCompute(
            mainCmdBuffer, UintDivAndCeil(brdfIntegralSize, 8), UintDivAndCeil(brdfIntegralSize, 8), 6);

        // transfer dst texture from general to shader read only layout
        gfxDevice.insertBarriers(
            mainCmdBuffer,
            {
                Barrier::FromImage{
                    .texture = brdfIntegralMap,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });
    }

    // TODO: getPtrTmp() function (explicitetly state temporary!)
    auto* basicPBRMaterial = resourceManager.get(resourceManager.getMaterial("PBRBasic"));
    basicPBRMaterial->setResource("irradianceTex", *resourceManager.get<ResourceIndex>(irradianceTexHandle));
    basicPBRMaterial->setResource("prefilterTex", *resourceManager.get<ResourceIndex>(prefilteredEnvMap));
    basicPBRMaterial->setResource("brdfLUT", *resourceManager.get<ResourceIndex>(brdfIntegralMap));

    auto defaultCube = resourceManager.getMesh("DefaultCube");

    /*
        todo:
            load by default on engine init!
    */
    auto equiSkyboxMat = resourceManager.createMaterial({
        .vertexShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSky.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSkyEqui.frag"},
        .debugName = "equiSkyboxMat",
    });
    auto equiSkyboxMatInst = resourceManager.createMaterialInstance(equiSkyboxMat);
    {
        auto* inst = resourceManager.get(equiSkyboxMatInst);
        inst->setResource("equirectangularMap", *resourceManager.get<ResourceIndex>(hdri));
    }

    auto cubeSkyboxMat = resourceManager.createMaterial({
        .vertexShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSky.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSkyCube.frag"},
        .debugName = "cubeSkyboxMat",
    });

    auto cubeSkyboxMatInst = resourceManager.createMaterialInstance(cubeSkyboxMat);
    {
        auto* inst = resourceManager.get(cubeSkyboxMatInst);
        inst->setResource("cubeMap", *resourceManager.get<ResourceIndex>(hdriCube));
        // inst->setResource("cubeMap", resourceManager.get(irradianceTexHandle)->sampledResourceIndex);
    }

    // todo: createEntity takes initial set of components as arguments and returns all the pointers as []-thingy
    auto skybox = ecs.createEntity();
    {
        auto* transform = skybox.addComponent<Transform>();
        auto* renderInfo = skybox.addComponent<RenderInfo>();
        renderInfo->mesh = defaultCube;
        // renderInfo->materialInstance = equiSkyboxMatInst;
        renderInfo->materialInstance = cubeSkyboxMatInst;
    }

    // ------------------------------------------------------------------------------

    gfxDevice.enableValidationErrorBreakpoint();

    gfxDevice.endCommandBuffer(mainCmdBuffer);

    VkCommandBuffer materialUpdateCmds = updateDirtyMaterialParameters();

    gfxDevice.submitInitializationWork({materialUpdateCmds, mainCmdBuffer});

    // just to be safe, wait for all commands to be done here
    gfxDevice.waitForWorkFinished();

    gfxDevice.enableValidationErrorBreakpoint();
}

Editor::~Editor()
{
    while(threadPool.busy())
        ;
    threadPool.stop();

    // TODO: need a fancy way of ensuring that this is always called in applications
    //       Cant use base class destructor since that will only be called *after*wards
    gfxDevice.waitForWorkFinished();
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
    gfxDevice.startNextFrame();

    // Draw Scene ------
    auto recordSceneDrawingCommands = [&](int threadIndex)
    {
        VkCommandBuffer offscreenCmdBuffer = gfxDevice.beginCommandBuffer(threadIndex);

        gfxDevice.insertBarriers(
            offscreenCmdBuffer,
            {
                Barrier::FromImage{
                    .texture = depthTexture,
                    .stateBefore = ResourceState::DepthStencilTarget,
                    .stateAfter = ResourceState::DepthStencilTarget,
                    .allowDiscardOriginal = true,
                },
            });
        gfxDevice.insertSwapchainImageBarrier(
            offscreenCmdBuffer, ResourceState::OldSwapchainImage, ResourceState::Rendertarget);

        gfxDevice.beginRendering(
            offscreenCmdBuffer,
            {ColorTarget{.texture = ColorTarget::SwapchainImage{}, .loadOp = RenderTarget::LoadOp::Clear}},
            DepthTarget{.texture = depthTexture, .loadOp = RenderTarget::LoadOp::Clear});

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

        memcpy(renderPassDataBuffer->gpuBuffer.ptr, &renderPassData, sizeof(renderPassData));

        Buffer* objectBuffer = resourceManager.get(getCurrentFrameData().objectBuffer);

        void* objectData = objectBuffer->gpuBuffer.ptr;
        // not sure how good assigning single GPUObjectDatas is (vs CPU buffer and then one memcpy)
        auto* objectSSBO = (GPUObjectData*)objectData;
        for(int i = 0; i < renderables.size(); i++)
        {
            const RenderObject& object = renderables[i];
            objectSSBO[i].modelMatrix = object.transformMatrix;
        }

        //---

        BindlessIndices pushConstants;
        pushConstants.RenderInfoBuffer = renderPassDataBuffer->gpuBuffer.resourceIndex;
        pushConstants.transformBuffer = objectBuffer->gpuBuffer.resourceIndex;

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
                    gfxDevice.setGraphicsPipelineState(offscreenCmdBuffer, newMat->pipeline);
                    Buffer* materialParamsBuffer = resourceManager.get(newMat->parameters.deviceBuffer);
                    if(materialParamsBuffer != nullptr)
                        pushConstants.materialParamsBuffer = materialParamsBuffer->gpuBuffer.resourceIndex;
                    else
                        pushConstants.materialParamsBuffer = 0xFFFFFFFF;
                    lastMaterial = newMatInst->parentMaterial;
                }

                Buffer* materialInstanceParamsBuffer = resourceManager.get(newMatInst->parameters.deviceBuffer);
                if(materialInstanceParamsBuffer != nullptr)
                    pushConstants.materialInstanceParamsBuffer =
                        materialInstanceParamsBuffer->gpuBuffer.resourceIndex;
                else
                    pushConstants.materialInstanceParamsBuffer = 0xFFFFFFFF;
            }

            pushConstants.transformIndex = i;
            gfxDevice.pushConstants(offscreenCmdBuffer, sizeof(BindlessIndices), &pushConstants);

            if(objectMesh != lastMesh)
            {
                Mesh* newMesh = resourceManager.get(objectMesh);
                indexCount = newMesh->indexCount;
                Buffer* indexBuffer = resourceManager.get(newMesh->indexBuffer);
                Buffer* positionBuffer = resourceManager.get(newMesh->positionBuffer);
                Buffer* attributeBuffer = resourceManager.get(newMesh->attributeBuffer);
                gfxDevice.bindIndexBuffer(offscreenCmdBuffer, newMesh->indexBuffer);
                gfxDevice.bindVertexBuffers(
                    offscreenCmdBuffer, 0, 2, {newMesh->positionBuffer, newMesh->attributeBuffer}, {0, 0});
                lastMesh = objectMesh;
            }

            gfxDevice.drawIndexed(offscreenCmdBuffer, indexCount, 1, 0, 0, 0);
        }

        gfxDevice.endRendering(offscreenCmdBuffer);
        gfxDevice.endCommandBuffer(offscreenCmdBuffer);

        return offscreenCmdBuffer;
    };

    // Draw UI ---------
    auto recordUIdrawingCommands = [&](int threadIndex)
    {
        VkCommandBuffer onscreenCmdBuffer = gfxDevice.beginCommandBuffer(threadIndex);
        gfxDevice.beginRendering(
            onscreenCmdBuffer,
            {ColorTarget{.texture = ColorTarget::SwapchainImage{}, .loadOp = RenderTarget::LoadOp::Load}},
            DepthTarget{.texture = Texture::Handle::Null()});
        gfxDevice.drawImGui(onscreenCmdBuffer);
        gfxDevice.endRendering(onscreenCmdBuffer);

        gfxDevice.insertSwapchainImageBarrier(
            onscreenCmdBuffer, ResourceState::Rendertarget, ResourceState::PresentSrc);

        gfxDevice.endCommandBuffer(onscreenCmdBuffer);

        return onscreenCmdBuffer;
    };

    VkCommandBuffer materialParamUpdates = updateDirtyMaterialParameters();

    auto offscreenFuture = threadPool.queueJob(recordSceneDrawingCommands);
    auto onscreenFuture = threadPool.queueJob(recordUIdrawingCommands);

    VkCommandBuffer offscreenCmdBuffer = offscreenFuture.get();
    VkCommandBuffer onscreenCmdBuffer = onscreenFuture.get();

    gfxDevice.submitCommandBuffers({materialParamUpdates, offscreenCmdBuffer, onscreenCmdBuffer});

    gfxDevice.presentSwapchain();

    // ------------------------------
}

VkCommandBuffer Editor::updateDirtyMaterialParameters()
{
    VkCommandBuffer materialUpdateCmds = gfxDevice.beginCommandBuffer();

    auto& materials = resourceManager.getMaterialPool();
    // TODO: MERGE ALL BARRIER SUBMITS !!
    for(auto iter = materials.begin(); iter != materials.end(); iter++)
    {
        auto* material = *iter;
        if(!material->dirty)
            continue;
        gfxDevice.insertBarriers(
            materialUpdateCmds,
            {
                Barrier::FromBuffer{
                    .buffer = material->parameters.deviceBuffer,
                    .stateBefore = ResourceState::UniformBuffer,
                    .stateAfter = ResourceState::TransferDst,
                },
            });
        auto gpuAlloc = gfxDevice.allocateStagingData(material->parameters.bufferSize);
        memcpy(gpuAlloc.ptr, material->parameters.writeBuffer, material->parameters.bufferSize);
        gfxDevice.copyBuffer(
            materialUpdateCmds,
            gpuAlloc.buffer,
            gpuAlloc.offset,
            material->parameters.deviceBuffer,
            0,
            material->parameters.bufferSize);
        material->dirty = false;
        gfxDevice.insertBarriers(
            materialUpdateCmds,
            {
                Barrier::FromBuffer{
                    .buffer = material->parameters.deviceBuffer,
                    .stateBefore = ResourceState::TransferDst,
                    .stateAfter = ResourceState::UniformBuffer,
                },
            });
    }
    auto& materialInstances = resourceManager.getMaterialInstancePool();
    for(auto iter = materialInstances.begin(); iter != materialInstances.end(); iter++)
    {
        auto* materialInst = *iter;
        if(!materialInst->dirty)
            continue;
        gfxDevice.insertBarriers(
            materialUpdateCmds,
            {
                Barrier::FromBuffer{
                    .buffer = materialInst->parameters.deviceBuffer,
                    .stateBefore = ResourceState::UniformBuffer,
                    .stateAfter = ResourceState::TransferDst,
                },
            });
        auto gpuAlloc = gfxDevice.allocateStagingData(materialInst->parameters.bufferSize);
        memcpy(gpuAlloc.ptr, materialInst->parameters.writeBuffer, materialInst->parameters.bufferSize);
        gfxDevice.copyBuffer(
            materialUpdateCmds,
            gpuAlloc.buffer,
            gpuAlloc.offset,
            materialInst->parameters.deviceBuffer,
            0,
            materialInst->parameters.bufferSize);
        materialInst->dirty = false;
        gfxDevice.insertBarriers(
            materialUpdateCmds,
            {
                Barrier::FromBuffer{
                    .buffer = materialInst->parameters.deviceBuffer,
                    .stateBefore = ResourceState::TransferDst,
                    .stateAfter = ResourceState::UniformBuffer,
                },
            });
    }
    gfxDevice.endCommandBuffer(materialUpdateCmds);

    return materialUpdateCmds;
}