#include "Editor.hpp"
#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Barrier/Barrier.hpp>
#include <Engine/Graphics/Mesh/Cube.hpp>
#include <Engine/Graphics/Mesh/Fullscreen.hpp>
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
    ecs.registerComponent<GPURepr>();

    sceneRoot.addComponent<Transform>();
    sceneRoot.addComponent<Hierarchy>();

    // rendering internals --------------------------------------------

    // create this first, need to ensure resourceIndex is 0 (since thats currently hardcoded in the shaders)
    //  TODO: switch to spec constant?
    transformsBuffer = resourceManager.createBuffer(Buffer::CreateInfo{
        .debugName = "objectTransformsBuffer",
        .size = sizeof(glm::mat4) * MAX_GPU_OBJECTS,
        .memoryType = Buffer::MemoryType::GPU_BUT_CPU_VISIBLE,
        .allStates = ResourceState::Storage,
        .initialState = ResourceState::Storage,
    });
    assert(*resourceManager.get<ResourceIndex>(transformsBuffer) == 0);

    for(int i = 0; i < ArraySize(perFrameData); i++)
    {
        perFrameData[i].renderPassDataBuffer = resourceManager.createBuffer(Buffer::CreateInfo{
            .debugName = ("RenderPassDataBuffer" + std::to_string(i)),
            .size = sizeof(RenderPassData),
            .memoryType = Buffer::MemoryType::GPU_BUT_CPU_VISIBLE,
            .allStates = ResourceState::UniformBuffer,
            .initialState = ResourceState::UniformBuffer,
        });
    }

    depthTexture = resourceManager.createTexture(Texture::CreateInfo{
        .debugName = "Depth Texture",
        .format = depthFormat,
        .allStates = ResourceState::DepthStencilTarget,
        .initialState = ResourceState::Undefined,
        .size = {gfxDevice.getSwapchainWidth(), gfxDevice.getSwapchainHeight()},
    });

    offscreenTexture = resourceManager.createTexture(Texture::CreateInfo{
        .debugName = "Offscreen RT",
        .format = offscreenRTFormat,
        .allStates = ResourceState::Rendertarget | ResourceState::SampleSourceGraphics,
        .initialState = ResourceState::Rendertarget,
        .size = {gfxDevice.getSwapchainWidth(), gfxDevice.getSwapchainHeight()},
    });

    // default resources --------------------------------------------

    createDefaultSamplers();

    // needed for glTF loading
    resourceManager.createMaterial({
        .debugName = "PBRBasic",
        .vertexShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasic.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasic.frag"},
        .colorFormats = {offscreenRTFormat},
        .depthFormat = depthFormat,
    });
    assert(resourceManager.get<std::string>(resourceManager.getMaterial("PBRBasic")) != nullptr);

    resourceManager.createMesh(Cube::positions, Cube::attributes, Cube::indices, "DefaultCube");
    assert(resourceManager.get<std::string>(resourceManager.getMesh("DefaultCube")) != nullptr);

    fullscreenTri = resourceManager.createMesh(
        FullscreenTri::positions, FullscreenTri::attributes, FullscreenTri::indices, "FullscreenTri");
    assert(resourceManager.get<std::string>(fullscreenTri) != nullptr);

    mainCamera =
        Camera{static_cast<float>(mainWindow.width) / static_cast<float>(mainWindow.height), 0.1f, 1000.0f};

    glfwSetTime(0.0);
    inputManager.resetTime();

    // Scene and other test stuff loading -------------------------------------------
    VkCommandBuffer mainCmdBuffer = gfxDevice.beginCommandBuffer();

    // Disable validation error breakpoints during init, synch errors arent correct
    gfxDevice.disableValidationErrorBreakpoint();

    // TODO: execute this while GPU is already doing work, instead of waiting for this *then* starting GPU
    Scene::load("C:/Users/jonas/Documents/Models/DamagedHelmet/DamagedHelmet.gltf", &ecs, sceneRoot);

    createDefaultTextures(mainCmdBuffer);

    equiToCubeShader = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/Skybox/equiToCube.hlsl"}, "equiToCubeCompute");

    irradianceCalcShader = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/Skybox/generateIrradiance.comp"}, "generateIrradiance");

    auto unlitTexturedMaterial = resourceManager.createMaterial({
        .debugName = "texturedUnlit",
        .vertexShader = {.sourcePath = SHADERS_PATH "/Unlit/TexturedUnlit.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/Unlit/TexturedUnlit.frag"},
        .colorFormats = {offscreenRTFormat},
        .depthFormat = depthFormat,
    });

    SkyboxTextures skyboxTextures = generateSkyboxTextures(mainCmdBuffer, defaultHDRI, 512, 32, 128);

    auto unlitMatInst = resourceManager.createMaterialInstance(unlitTexturedMaterial);
    MaterialInstance::setResource(unlitMatInst, "texture", *resourceManager.get<ResourceIndex>(mipDebugTex));

    auto writeToSwapchainMat = resourceManager.createMaterial({
        .debugName = "writeToSwapchain",
        .vertexShader = {.sourcePath = SHADERS_PATH "/WriteToSwapchain/WriteToSwapchain.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/WriteToSwapchain/WriteToSwapchain.frag"},
        .colorFormats = {Texture::Format::B8_G8_R8_A8_SRGB},
    });
    writeToSwapchainMatInst = resourceManager.createMaterialInstance(writeToSwapchainMat);
    MaterialInstance::setResource(
        writeToSwapchainMatInst, "texture", *resourceManager.get<ResourceIndex>(offscreenTexture));

    Material::Handle pbrMat = resourceManager.getMaterial("PBRBasic");
    Material::setResource(
        pbrMat, "irradianceTex", *resourceManager.get<ResourceIndex>(skyboxTextures.irradianceMap));
    Material::setResource(
        pbrMat, "prefilterTex", *resourceManager.get<ResourceIndex>(skyboxTextures.prefilteredMap));
    Material::setResource(pbrMat, "brdfLUT", *resourceManager.get<ResourceIndex>(brdfIntegralTex));

    auto equiSkyboxMat = resourceManager.createMaterial({
        .debugName = "equiSkyboxMat",
        .vertexShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSky.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSkyEqui.frag"},
        .colorFormats = {offscreenRTFormat},
        .depthFormat = depthFormat,
    });
    auto equiSkyboxMatInst = resourceManager.createMaterialInstance(equiSkyboxMat);
    {
        MaterialInstance::setResource(
            equiSkyboxMatInst, "equirectangularMap", *resourceManager.get<ResourceIndex>(defaultHDRI));
    }

    auto cubeSkyboxMat = resourceManager.createMaterial({
        .debugName = "cubeSkyboxMat",
        .vertexShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSky.vert"},
        .fragmentShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSkyCube.frag"},
        .colorFormats = {offscreenRTFormat},
        .depthFormat = depthFormat,
    });

    auto cubeSkyboxMatInst = resourceManager.createMaterialInstance(cubeSkyboxMat);
    {
        MaterialInstance::setResource(
            cubeSkyboxMatInst, "cubeMap", *resourceManager.get<ResourceIndex>(skyboxTextures.cubeMap));
    }

    auto skybox = ecs.createEntity();
    {
        auto* transform = skybox.addComponent<Transform>();
        auto* renderInfo = skybox.addComponent<RenderInfo>();
        renderInfo->mesh = resourceManager.getMesh("DefaultCube");
        // renderInfo->materialInstance = equiSkyboxMatInst;
        renderInfo->materialInstance = cubeSkyboxMatInst;
        auto* gpuRepr = skybox.addComponent<GPURepr>();
    }

    Mesh::Handle triangleMesh;
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

    auto newRenderable = ecs.createEntity();
    {
        auto* renderInfo = newRenderable.addComponent<RenderInfo>();
        renderInfo->mesh = triangleMesh;
        renderInfo->materialInstance = unlitMatInst;
        auto* transform = newRenderable.addComponent<Transform>();
        transform->position = glm::vec3{3.0f, 0.0f, 0.0f};
        transform->calculateLocalTransformMatrix();
        // dont like having to call this manually
        transform->localToWorld = transform->localTransform;
        /*
            maybe a wrapper around entt + default component creation like
            Scene::addObject() that just conatins Transform+Hierarchy component (and default parenting
            settings) would be nice
        */
        assert(renderInfo->mesh.isValid());
        assert(renderInfo->materialInstance.isValid());

        auto* gpuRepr = newRenderable.addComponent<GPURepr>();
        assert(gpuRepr->isDirty);
    }

    // ------------------------ Build transform buffer --------------------------------------------------

    // not sure how good assigning single GPUObjectDatas is (vs CPU buffer and then one memcpy)
    auto* gpuPtr = (glm::mat4*)(*resourceManager.get<void*>(transformsBuffer));
    ecs.forEach<RenderInfo, Transform, GPURepr>(
        [&](RenderInfo* renderinfo, Transform* transform, GPURepr* gpuRepr)
        {
            auto* name = resourceManager.get<std::string>(renderinfo->mesh);
            auto freeIndex = freeTransformIndex;

            assert(gpuRepr->isDirty);
            assert(gpuRepr->index == 0xFFFFFFFF);
            gpuPtr[freeTransformIndex] = transform->localToWorld;
            gpuRepr->index = freeTransformIndex;
            gpuRepr->isDirty = false;
            freeTransformIndex++;
        });

    // ------------------------------------------------------------------------------

    gfxDevice.enableValidationErrorBreakpoint();

    gfxDevice.endCommandBuffer(mainCmdBuffer);

    VkCommandBuffer materialUpdateCmds = updateDirtyMaterialParameters();

    gfxDevice.submitInitializationWork({materialUpdateCmds, mainCmdBuffer});

    // just to be safe, wait for all commands to be done here
    gfxDevice.waitForWorkFinished();

    gfxDevice.enableValidationErrorBreakpoint();
}

void Editor::createDefaultSamplers()
{
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
}

void Editor::createDefaultTextures(VkCommandBuffer cmd)
{
    Handle<ComputeShader> debugMipFillShaderH = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/Misc/debugMipFill.comp"}, "debugMipFill");
    ComputeShader* debugMipFillShader = resourceManager.get(debugMipFillShaderH);

    mipDebugTex = resourceManager.createTexture({
        .debugName = "mipTest",
        .type = Texture::Type::t2D,
        .format = Texture::Format::R8_G8_B8_A8_UNORM,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {128, 128},
        .mipLevels = Texture::MipLevels::All,
    });
    {
        gfxDevice.startDebugRegion(cmd, "Mip test tex filling");
        gfxDevice.setComputePipelineState(cmd, resourceManager.get(debugMipFillShaderH)->pipeline);

        struct DebugMipFillPushConstants
        {
            uint32_t targetIndex;
            uint32_t level;
        };
        DebugMipFillPushConstants constants{
            .targetIndex = 0xFFFFFFFF,
            .level = 0,
        };

        uint32_t mipCount = resourceManager.get<Texture::Descriptor>(mipDebugTex)->mipLevels;
        for(uint32_t mip = 0; mip < mipCount; mip++)
        {
            Handle<TextureView> mipView = gfxDevice.createTextureView(TextureView::CreateInfo{
                .parent = mipDebugTex,
                .allStates = ResourceState::StorageCompute,
                .baseMipLevel = mip,
            });

            uint32_t mipSize = glm::max(128u >> mip, 1u);

            constants = {
                .targetIndex = gfxDevice.get(mipView)->resourceIndex,
                .level = mip,
            };

            gfxDevice.pushConstants(cmd, sizeof(DebugMipFillPushConstants), &constants);

            // TODO: dont hardcode sizes! retrieve programmatically (workrgoup size form spirv)
            gfxDevice.dispatchCompute(cmd, UintDivAndCeil(mipSize, 8), UintDivAndCeil(mipSize, 8), 6);

            gfxDevice.destroy(mipView);
        }

        // transfer dst texture from general to shader read only layout
        gfxDevice.insertBarriers(
            cmd,
            {
                Barrier::FromImage{
                    .texture = mipDebugTex,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });
        gfxDevice.endDebugRegion(cmd);
    }

    defaultHDRI = resourceManager.createTexture(Texture::LoadInfo{
        .path = ASSETS_PATH "/HDRIs/kloppenheim_04_2k.hdr",
        .fileDataIsLinear = true,
        .allStates = ResourceState::SampleSource,
        .initialState = ResourceState::SampleSource,
    });

    Handle<ComputeShader> integrateBrdfShaderHandle = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/PBR/integrateBRDF.comp"}, "integrateBRDFComp");
    ComputeShader* integrateBRDFShader = resourceManager.get(integrateBrdfShaderHandle);
    uint32_t brdfIntegralSize = 512;
    brdfIntegralTex = resourceManager.createTexture({
        .debugName = "brdfIntegral",
        .type = Texture::Type::t2D,
        .format = Texture::Format::R16_G16_FLOAT,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {brdfIntegralSize, brdfIntegralSize},
        .mipLevels = 1,
    });
    {
        gfxDevice.setComputePipelineState(cmd, resourceManager.get(integrateBrdfShaderHandle)->pipeline);

        struct IntegratePushConstants
        {
            uint32_t outTexture;
        };
        IntegratePushConstants constants{*resourceManager.get<ResourceIndex>(brdfIntegralTex)};

        gfxDevice.pushConstants(cmd, sizeof(IntegratePushConstants), &constants);

        gfxDevice.dispatchCompute(
            cmd, UintDivAndCeil(brdfIntegralSize, 8), UintDivAndCeil(brdfIntegralSize, 8), 6);

        // transfer dst texture from general to shader read only layout
        gfxDevice.insertBarriers(
            cmd,
            {
                Barrier::FromImage{
                    .texture = brdfIntegralTex,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });
    }
}

Editor::SkyboxTextures Editor::generateSkyboxTextures(
    VkCommandBuffer cmd,
    Texture::Handle equiSource,
    uint32_t hdriCubeRes,
    uint32_t irradianceRes,
    uint32_t prefilteredEnvMapBaseSize)
{
    std::string baseName = *resourceManager.get<std::string>(equiSource);

    auto hdriCube = resourceManager.createTexture(Texture::CreateInfo{
        // specifying non-default values only
        .debugName = baseName + "Cubemap",
        .type = Texture::Type::tCube,
        .format = resourceManager.get<Texture::Descriptor>(equiSource)->format,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {.width = hdriCubeRes, .height = hdriCubeRes},
        .mipLevels = Texture::MipLevels::All,
    });
    {
        ComputeShader* conversionShader = resourceManager.get(equiToCubeShader);

        gfxDevice.setComputePipelineState(cmd, resourceManager.get(equiToCubeShader)->pipeline);

        struct ConversionPushConstants
        {
            uint32_t sourceIndex;
            uint32_t targetIndex;
        } constants = {
            .sourceIndex = *resourceManager.get<ResourceIndex>(equiSource),
            // TODO: Not sure if writing into image view with all levels correctly writes into 0th level
            .targetIndex = *resourceManager.get<ResourceIndex>(hdriCube),
        };
        gfxDevice.pushConstants(cmd, sizeof(ConversionPushConstants), &constants);

        gfxDevice.dispatchCompute(cmd, hdriCubeRes / 8, hdriCubeRes / 8, 6);

        gfxDevice.insertBarriers(
            cmd,
            {
                Barrier::FromImage{
                    .texture = hdriCube,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });

        gfxDevice.fillMipLevels(cmd, hdriCube, ResourceState::SampleSource);
    }

    auto irradianceTex = resourceManager.createTexture({
        .debugName = baseName + "Irradiance",
        .type = Texture::Type::tCube,
        .format = Texture::Format::R16_G16_B16_A16_FLOAT,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {irradianceRes, irradianceRes, 1},
    });
    {
        ComputeShader* calcIrradianceShader = resourceManager.get(irradianceCalcShader);
        gfxDevice.setComputePipelineState(cmd, calcIrradianceShader->pipeline);

        struct ConversionPushConstants
        {
            uint32_t sourceIndex;
            uint32_t targetIndex;
        };
        ConversionPushConstants constants{
            .sourceIndex = *resourceManager.get<ResourceIndex>(hdriCube),
            .targetIndex = *resourceManager.get<ResourceIndex>(irradianceTex),
        };

        gfxDevice.pushConstants(cmd, sizeof(ConversionPushConstants), &constants);

        gfxDevice.dispatchCompute(cmd, irradianceRes / 8, irradianceRes / 8, 6);

        // transfer dst texture from general to shader read only layout
        gfxDevice.insertBarriers(
            cmd,
            {
                Barrier::FromImage{
                    .texture = irradianceTex,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });
    }

    Handle<ComputeShader> prefilterEnvShaderHandle = resourceManager.createComputeShader(
        {.sourcePath = SHADERS_PATH "/Skybox/prefilter.comp"}, "prefilterEnvComp");
    ComputeShader* prefilterEnvShader = resourceManager.get(prefilterEnvShaderHandle);

    auto prefilteredEnvMap = resourceManager.createTexture({
        .debugName = baseName + "Prefiltered",
        .type = Texture::Type::tCube,
        .format = Texture::Format::R16_G16_B16_A16_FLOAT,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {prefilteredEnvMapBaseSize, prefilteredEnvMapBaseSize},
        .mipLevels = 5,
    });
    {
        gfxDevice.setComputePipelineState(cmd, resourceManager.get(prefilterEnvShaderHandle)->pipeline);

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

            gfxDevice.pushConstants(cmd, sizeof(PrefilterPushConstants), &constants);

            gfxDevice.dispatchCompute(cmd, UintDivAndCeil(mipSize, 8), UintDivAndCeil(mipSize, 8), 6);

            gfxDevice.destroy(mipView);
        }

        // transfer dst texture from general to shader read only layout
        gfxDevice.insertBarriers(
            cmd,
            {
                Barrier::FromImage{
                    .texture = prefilteredEnvMap,
                    .stateBefore = ResourceState::StorageCompute,
                    .stateAfter = ResourceState::SampleSource,
                },
            });
    }

    return SkyboxTextures{
        .cubeMap = hdriCube,
        .irradianceMap = irradianceTex,
        .prefilteredMap = prefilteredEnvMap,
    };
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

    // ANYTHING TOUCHING RENDERING RELATED RESOURCES NEEDS TO HAPPEN AFTER THIS !
    gfxDevice.startNextFrame();

    VkCommandBuffer materialParamUpdates = updateDirtyMaterialParameters();

    // update renderPassData
    RenderPassData renderPassData;
    renderPassData.proj = mainCamera.getProj();
    renderPassData.view = mainCamera.getView();
    renderPassData.projView = mainCamera.getProjView();
    renderPassData.cameraPositionWS = mainCamera.getPosition();
    void* renderPassDataPtr = *resourceManager.get<void*>(getCurrentFrameData().renderPassDataBuffer);
    memcpy(renderPassDataPtr, &renderPassData, sizeof(RenderPassData));

    auto offscreenFuture =
        threadPool.queueJob([editor = this](int threadIndex) { return editor->drawScene(threadIndex); });
    auto onscreenFuture =
        threadPool.queueJob([editor = this](int threadIndex) { return editor->drawUI(threadIndex); });

    VkCommandBuffer offscreenCmdBuffer = offscreenFuture.get();
    VkCommandBuffer onscreenCmdBuffer = onscreenFuture.get();

    gfxDevice.submitCommandBuffers({materialParamUpdates, offscreenCmdBuffer, onscreenCmdBuffer});

    gfxDevice.presentSwapchain();

    // ------------------------------
}

VkCommandBuffer Editor::drawScene(int threadIndex)
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
            Barrier::FromImage{
                .texture = offscreenTexture,
                .stateBefore = ResourceState::SampleSourceGraphics,
                .stateAfter = ResourceState::Rendertarget,
                .allowDiscardOriginal = true,
            },
        });

    gfxDevice.beginRendering(
        offscreenCmdBuffer,
        {ColorTarget{.texture = offscreenTexture, .loadOp = RenderTarget::LoadOp::Clear}},
        DepthTarget{.texture = depthTexture, .loadOp = RenderTarget::LoadOp::Clear});

    BindlessIndices pushConstants;
    pushConstants.RenderInfoBuffer =
        *resourceManager.get<ResourceIndex>(getCurrentFrameData().renderPassDataBuffer);

    Mesh::Handle lastMesh = Mesh::Handle::Invalid();
    Material::Handle lastMaterial = Material::Handle::Invalid();
    MaterialInstance::Handle lastMaterialInstance = MaterialInstance::Handle::Invalid();
    uint32_t indexCount = 0;

    ecs.forEach<RenderInfo, GPURepr>(
        [&](RenderInfo* renderinfo, GPURepr* gpuRepr)
        {
            assert(!gpuRepr->isDirty);
            assert(gpuRepr->index != 0xFFFFFFFF);

            Mesh::Handle objectMesh = renderinfo->mesh;
            MaterialInstance::Handle objectMaterialInstance = renderinfo->materialInstance;

            if(objectMaterialInstance != lastMaterialInstance)
            {
                Material::Handle newMaterial = *resourceManager.get<Material::Handle>(objectMaterialInstance);
                if(newMaterial != lastMaterial)
                {
                    gfxDevice.setGraphicsPipelineState(
                        offscreenCmdBuffer, *resourceManager.get<VkPipeline>(newMaterial));

                    auto* parameters = resourceManager.get<Material::ParameterBuffer>(newMaterial);
                    Buffer::Handle paramBuffer = parameters->deviceBuffer;
                    if(paramBuffer.isValid())
                        pushConstants.materialParamsBuffer = *resourceManager.get<ResourceIndex>(paramBuffer);
                    else
                        pushConstants.materialParamsBuffer = 0xFFFFFFFF;
                    lastMaterial = newMaterial;
                }

                Buffer::Handle paramBuffer =
                    resourceManager.get<MaterialInstance::ParameterBuffer>(objectMaterialInstance)->deviceBuffer;
                if(paramBuffer.isValid())
                    pushConstants.materialInstanceParamsBuffer = *resourceManager.get<ResourceIndex>(paramBuffer);
                else
                    pushConstants.materialInstanceParamsBuffer = 0xFFFFFFFF;
            }

            gfxDevice.pushConstants(offscreenCmdBuffer, sizeof(BindlessIndices), &pushConstants);

            if(objectMesh != lastMesh)
            {
                auto* meshData = resourceManager.get<Mesh::RenderData>(objectMesh);
                indexCount = meshData->indexCount;
                gfxDevice.bindIndexBuffer(offscreenCmdBuffer, meshData->indexBuffer);
                gfxDevice.bindVertexBuffers(
                    offscreenCmdBuffer, 0, 2, {meshData->positionBuffer, meshData->attributeBuffer}, {0, 0});
                lastMesh = objectMesh;
            }

            gfxDevice.drawIndexed(offscreenCmdBuffer, indexCount, 1, 0, 0, gpuRepr->index);
        });

    gfxDevice.endRendering(offscreenCmdBuffer);
    gfxDevice.endCommandBuffer(offscreenCmdBuffer);

    return offscreenCmdBuffer;
};

VkCommandBuffer Editor::drawUI(int threadIndex)
{
    VkCommandBuffer onscreenCmdBuffer = gfxDevice.beginCommandBuffer(threadIndex);

    gfxDevice.insertBarriers(
        onscreenCmdBuffer,
        {
            Barrier::FromImage{
                .texture = offscreenTexture,
                .stateBefore = ResourceState::Rendertarget,
                .stateAfter = ResourceState::SampleSourceGraphics,
            },
        });

    gfxDevice.insertSwapchainImageBarrier(
        onscreenCmdBuffer, ResourceState::OldSwapchainImage, ResourceState::Rendertarget);

    gfxDevice.beginRendering(
        onscreenCmdBuffer,
        {ColorTarget{.texture = ColorTarget::SwapchainImage{}, .loadOp = RenderTarget::LoadOp::Clear}},
        DepthTarget{.texture = Texture::Handle::Null()});

    // Transfer offscreen image to swapchain

    // TODO: abstraction for this!
    BindlessIndices pushConstants;
    pushConstants.RenderInfoBuffer =
        *resourceManager.get<ResourceIndex>(getCurrentFrameData().renderPassDataBuffer);

    Material::Handle newMaterial = *resourceManager.get<Material::Handle>(writeToSwapchainMatInst);

    gfxDevice.setGraphicsPipelineState(onscreenCmdBuffer, *resourceManager.get<VkPipeline>(newMaterial));

    Buffer::Handle paramBuffer =
        resourceManager.get<MaterialInstance::ParameterBuffer>(writeToSwapchainMatInst)->deviceBuffer;
    pushConstants.materialInstanceParamsBuffer = *resourceManager.get<ResourceIndex>(paramBuffer);

    gfxDevice.pushConstants(onscreenCmdBuffer, sizeof(BindlessIndices), &pushConstants);

    auto* meshData = resourceManager.get<Mesh::RenderData>(fullscreenTri);
    gfxDevice.bindIndexBuffer(onscreenCmdBuffer, meshData->indexBuffer);
    gfxDevice.bindVertexBuffers(
        onscreenCmdBuffer, 0, 2, {meshData->positionBuffer, meshData->attributeBuffer}, {0, 0});
    gfxDevice.drawIndexed(onscreenCmdBuffer, meshData->indexCount, 1, 0, 0, 0);

    gfxDevice.drawImGui(onscreenCmdBuffer);

    gfxDevice.endRendering(onscreenCmdBuffer);

    gfxDevice.insertSwapchainImageBarrier(
        onscreenCmdBuffer, ResourceState::Rendertarget, ResourceState::PresentSrc);

    gfxDevice.endCommandBuffer(onscreenCmdBuffer);

    return onscreenCmdBuffer;
}

VkCommandBuffer Editor::updateDirtyMaterialParameters()
{
    VkCommandBuffer materialUpdateCmds = gfxDevice.beginCommandBuffer();

    auto& materials = resourceManager.getMaterialPool();
    // TODO: MERGE ALL BARRIER SUBMITS !!
    for(auto iter = materials.begin(); iter != materials.end(); iter++)
    {
        auto handle = *iter;
        bool* dirtyFlag = resourceManager.get<bool>(handle);
        if(!*dirtyFlag)
            continue;
        Material::ParameterBuffer& paramBuffer = *resourceManager.get<Material::ParameterBuffer>(handle);
        gfxDevice.insertBarriers(
            materialUpdateCmds,
            {
                Barrier::FromBuffer{
                    .buffer = paramBuffer.deviceBuffer,
                    .stateBefore = ResourceState::UniformBuffer,
                    .stateAfter = ResourceState::TransferDst,
                },
            });
        auto gpuAlloc = gfxDevice.allocateStagingData(paramBuffer.size);
        memcpy(gpuAlloc.ptr, paramBuffer.writeBuffer, paramBuffer.size);
        gfxDevice.copyBuffer(
            materialUpdateCmds, gpuAlloc.buffer, gpuAlloc.offset, paramBuffer.deviceBuffer, 0, paramBuffer.size);
        *dirtyFlag = false;
        gfxDevice.insertBarriers(
            materialUpdateCmds,
            {
                Barrier::FromBuffer{
                    .buffer = paramBuffer.deviceBuffer,
                    .stateBefore = ResourceState::TransferDst,
                    .stateAfter = ResourceState::UniformBuffer,
                },
            });
    }
    auto& materialInstances = resourceManager.getMaterialInstancePool();
    for(auto iter = materialInstances.begin(); iter != materialInstances.end(); iter++)
    {
        auto handle = *iter;
        bool* dirtyFlag = resourceManager.get<bool>(handle);
        if(!*dirtyFlag)
            continue;
        MaterialInstance::ParameterBuffer& paramBuffer =
            *resourceManager.get<MaterialInstance::ParameterBuffer>(handle);
        gfxDevice.insertBarriers(
            materialUpdateCmds,
            {
                Barrier::FromBuffer{
                    .buffer = paramBuffer.deviceBuffer,
                    .stateBefore = ResourceState::UniformBuffer,
                    .stateAfter = ResourceState::TransferDst,
                },
            });
        auto gpuAlloc = gfxDevice.allocateStagingData(paramBuffer.size);
        memcpy(gpuAlloc.ptr, paramBuffer.writeBuffer, paramBuffer.size);
        gfxDevice.copyBuffer(
            materialUpdateCmds, gpuAlloc.buffer, gpuAlloc.offset, paramBuffer.deviceBuffer, 0, paramBuffer.size);
        *dirtyFlag = false;
        gfxDevice.insertBarriers(
            materialUpdateCmds,
            {
                Barrier::FromBuffer{
                    .buffer = paramBuffer.deviceBuffer,
                    .stateBefore = ResourceState::TransferDst,
                    .stateAfter = ResourceState::UniformBuffer,
                },
            });
    }
    gfxDevice.endCommandBuffer(materialUpdateCmds);

    return materialUpdateCmds;
}