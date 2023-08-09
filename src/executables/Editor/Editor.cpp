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

    Scene::load("C:/Users/jonas/Documents/Models/DamagedHelmet/DamagedHelmet.gltf", &ecs, sceneRoot);

    Handle<ComputeShader> debugMipFillShaderH =
        rm->createComputeShader({.sourcePath = SHADERS_PATH "/Misc/debugMipFill.comp"}, "debugMipFill");
    ComputeShader* debugMipFillShader = rm->get(debugMipFillShaderH);

    auto mipTestTexH = rm->createTexture({
        .debugName = "mipTest",
        .type = Texture::Type::t2D,
        .format = Texture::Format::r8g8b8a8_unorm,
        .allStates = ResourceState::SampleSource | ResourceState::StorageComputeWrite,
        .initialState = ResourceState::StorageComputeWrite,
        .size = {128, 128},
        .mipLevels = Texture::MipLevels::All,
    });
    {
        Texture* mipTestTex = rm->get(mipTestTexH);

        VulkanRenderer* renderer = Engine::get()->getRenderer();
        renderer->immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                startDebugRegion(cmd, "Mip test tex filling");
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, debugMipFillShader->pipeline);

                // Bind the bindless descriptor sets once per cmdbuffer
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    renderer->bindlessPipelineLayout,
                    0,
                    renderer->bindlessManager.getDescriptorSetsCount(),
                    renderer->bindlessManager.getDescriptorSets(),
                    0,
                    nullptr);

                struct DebugMipFillPushConstants
                {
                    uint32_t targetIndex;
                    uint32_t level;
                };
                DebugMipFillPushConstants constants{
                    .targetIndex = mipTestTex->mipResourceIndex(0),
                    .level = 0,
                };

                uint32_t mipCount = mipTestTex->descriptor.mipLevels;
                for(uint32_t mip = 0; mip < mipCount; mip++)
                {
                    uint32_t mipSize = glm::max(128u >> mip, 1u);

                    constants = {
                        .targetIndex = mipTestTex->mipResourceIndex(mip),
                        .level = mip,
                    };

                    // TODO: CORRECT PUSH CONSTANT RANGES FOR COMPUTE PIPELINES !!!!!
                    vkCmdPushConstants(
                        cmd,
                        renderer->bindlessPipelineLayout,
                        VK_SHADER_STAGE_ALL,
                        0,
                        sizeof(DebugMipFillPushConstants),
                        &constants);

                    // TODO: dont hardcode sizes! retrieve programmatically (workrgoup size form spirv)
                    vkCmdDispatch(cmd, UintDivAndCeil(mipSize, 8), UintDivAndCeil(mipSize, 8), 6);
                }

                // transfer dst texture from general to shader read only layout
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = mipTestTexH,
                            .stateBefore = ResourceState::StorageComputeWrite,
                            .stateAfter = ResourceState::SampleSource,
                        }),
                    });
                endDebugRegion(cmd);
            });
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

        triangleMesh = rm->createMesh(triangleVertexPositions, triangleVertexAttributes, {}, "triangle");
    }

    auto unlitTexturedMaterialH = rm->createMaterial(
        {
            .vertexShader = {.sourcePath = SHADERS_PATH "/Unlit/TexturedUnlit.vert"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/Unlit/TexturedUnlit.frag"},
        },
        "texturedUnlit");

    auto unlitTexturedMaterial = rm->createMaterialInstance(unlitTexturedMaterialH);
    rm->get(unlitTexturedMaterial)->parameters.setResource("texture", rm->get(mipTestTexH)->fullResourceIndex());
    rm->get(unlitTexturedMaterial)->parameters.pushChanges();

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

    auto hdri = engine.getResourceManager()->createTexture(Texture::LoadInfo{
        .path = ASSETS_PATH "/HDRIs/kloppenheim_04_2k.hdr",
        .fileDataIsLinear = true,
        .allStates = ResourceState::SampleSource,
        .initialState = ResourceState::SampleSource,
    });
    const int32_t hdriCubeRes = 512;
    auto hdriCube = rm->createTexture(Texture::CreateInfo{
        // specifying non-default values only
        .debugName = "HdriCubemap",
        .type = Texture::Type::tCube,
        .format = rm->get(hdri)->descriptor.format,
        .allStates = ResourceState::SampleSource | ResourceState::StorageComputeWrite,
        .initialState = ResourceState::StorageComputeWrite,
        .size = {.width = hdriCubeRes, .height = hdriCubeRes},
        .mipLevels = Texture::MipLevels::All,
    });
    Handle<ComputeShader> conversionShaderHandle =
        rm->createComputeShader({.sourcePath = SHADERS_PATH "/Skybox/equiToCube.hlsl"}, "equiToCubeCompute");
    ComputeShader* conversionShader = rm->get(conversionShaderHandle);

    {
        auto* renderer = Engine::get()->getRenderer();
        renderer->immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, conversionShader->pipeline);
                // Bind the bindless descriptor sets once per cmdbuffer
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    renderer->bindlessPipelineLayout,
                    0,
                    renderer->bindlessManager.getDescriptorSetsCount(),
                    renderer->bindlessManager.getDescriptorSets(),
                    0,
                    nullptr);

                struct ConversionPushConstants
                {
                    uint32_t sourceIndex;
                    uint32_t targetIndex;
                } constants = {
                    .sourceIndex = rm->get(hdri)->fullResourceIndex(),
                    .targetIndex = rm->get(hdriCube)->mipResourceIndex(0),
                };
                // TODO: CORRECT PUSH CONSTANT RANGES FOR COMPUTE PIPELINES !!!!!
                vkCmdPushConstants(
                    cmd,
                    renderer->bindlessPipelineLayout,
                    VK_SHADER_STAGE_ALL,
                    0,
                    sizeof(ConversionPushConstants),
                    &constants);

                // TODO: dont hardcode sizes! retrieve programmatically
                //       (workrgoup size form spirv, use UintDivAndCeil)
                vkCmdDispatch(cmd, hdriCubeRes / 8, hdriCubeRes / 8, 6);

                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = hdriCube,
                            .stateBefore = ResourceState::StorageComputeWrite,
                            .stateAfter = ResourceState::SampleSource,
                        }),
                    });
            });

        Texture::fillMipLevels(hdriCube, ResourceState::SampleSource);
    }

    Handle<ComputeShader> calcIrradianceComp = rm->createComputeShader(
        {.sourcePath = SHADERS_PATH "/Skybox/generateIrradiance.comp"}, "generateIrradiance");
    ComputeShader* calcIrradianceShader = rm->get(calcIrradianceComp);

    uint32_t irradianceRes = 32;
    auto irradianceTexHandle = rm->createTexture({
        .debugName = "hdriIrradiance",
        .type = Texture::Type::tCube,
        .format = Texture::Format::r16g16b16a16_float,
        .allStates = ResourceState::SampleSource | ResourceState::StorageComputeWrite,
        .initialState = ResourceState::StorageComputeWrite,
        .size = {irradianceRes, irradianceRes, 1},
    });
    {
        Texture* irradianceTex = rm->get(irradianceTexHandle);

        VulkanRenderer* renderer = Engine::get()->getRenderer();
        renderer->immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, calcIrradianceShader->pipeline);

                // Bind the bindless descriptor sets once per cmdbuffer
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    renderer->bindlessPipelineLayout,
                    0,
                    renderer->bindlessManager.getDescriptorSetsCount(),
                    renderer->bindlessManager.getDescriptorSets(),
                    0,
                    nullptr);

                struct ConversionPushConstants
                {
                    uint32_t sourceIndex;
                    uint32_t targetIndex;
                };
                ConversionPushConstants constants{
                    .sourceIndex = rm->get(hdriCube)->fullResourceIndex(),
                    .targetIndex = irradianceTex->mipResourceIndex(0),
                };

                // TODO: CORRECT PUSH CONSTANT RANGES FOR COMPUTE PIPELINES !!!!!
                vkCmdPushConstants(
                    cmd,
                    renderer->bindlessPipelineLayout,
                    VK_SHADER_STAGE_ALL,
                    0,
                    sizeof(ConversionPushConstants),
                    &constants);

                // TODO: dont hardcode sizes! retrieve programmatically
                //       (workrgoup size form spirv, use UintDivAndCeil)
                vkCmdDispatch(cmd, irradianceRes / 8, irradianceRes / 8, 6);

                // transfer dst texture from general to shader read only layout
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = irradianceTexHandle,
                            .stateBefore = ResourceState::StorageComputeWrite,
                            .stateAfter = ResourceState::SampleSource,
                        }),
                    });
            });
    }

    Handle<ComputeShader> prefilterEnvShaderHandle =
        rm->createComputeShader({.sourcePath = SHADERS_PATH "/Skybox/prefilter.comp"}, "prefilterEnvComp");
    ComputeShader* prefilterEnvShader = rm->get(prefilterEnvShaderHandle);

    uint32_t prefilteredEnvMapBaseSize = 128;
    auto prefilteredEnvMap = rm->createTexture({
        .debugName = "prefilteredEnvMap",
        .type = Texture::Type::tCube,
        .format = Texture::Format::r16g16b16a16_float,
        .allStates = ResourceState::SampleSource | ResourceState::StorageComputeWrite,
        .initialState = ResourceState::StorageComputeWrite,
        .size = {prefilteredEnvMapBaseSize, prefilteredEnvMapBaseSize},
        .mipLevels = 5,
    });
    {
        Texture* prefilteredEnv = rm->get(prefilteredEnvMap);

        VulkanRenderer* renderer = Engine::get()->getRenderer();
        renderer->immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prefilterEnvShader->pipeline);

                // Bind the bindless descriptor sets once per cmdbuffer
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    renderer->bindlessPipelineLayout,
                    0,
                    renderer->bindlessManager.getDescriptorSetsCount(),
                    renderer->bindlessManager.getDescriptorSets(),
                    0,
                    nullptr);

                struct PrefilterPushConstants
                {
                    uint32_t sourceIndex;
                    uint32_t targetIndex;
                    float roughness;
                };
                PrefilterPushConstants constants{
                    .sourceIndex = rm->get(hdriCube)->fullResourceIndex(),
                    .targetIndex = prefilteredEnv->mipResourceIndex(0),
                    .roughness = 0.0,
                };

                uint32_t mipCount = prefilteredEnv->descriptor.mipLevels;
                for(uint32_t mip = 0; mip < mipCount; mip++)
                {
                    uint32_t mipSize = glm::max(prefilteredEnvMapBaseSize >> mip, 1u);

                    constants = {
                        .sourceIndex = rm->get(hdriCube)->fullResourceIndex(),
                        .targetIndex = prefilteredEnv->mipResourceIndex(mip),
                        .roughness = float(mip) / float(mipCount - 1),
                    };

                    // TODO: CORRECT PUSH CONSTANT RANGES FOR COMPUTE PIPELINES !!!!!
                    vkCmdPushConstants(
                        cmd,
                        renderer->bindlessPipelineLayout,
                        VK_SHADER_STAGE_ALL,
                        0,
                        sizeof(PrefilterPushConstants),
                        &constants);

                    // TODO: dont hardcode sizes! retrieve programmatically (workrgoup size form spirv)
                    vkCmdDispatch(cmd, UintDivAndCeil(mipSize, 8), UintDivAndCeil(mipSize, 8), 6);
                }

                // transfer dst texture from general to shader read only layout
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = prefilteredEnvMap,
                            .stateBefore = ResourceState::StorageComputeWrite,
                            .stateAfter = ResourceState::SampleSource,
                        }),
                    });
            });
    }

    Handle<ComputeShader> integrateBrdfShaderHandle =
        rm->createComputeShader({.sourcePath = SHADERS_PATH "/PBR/integrateBRDF.comp"}, "integrateBRDFComp");
    ComputeShader* integrateBRDFShader = rm->get(integrateBrdfShaderHandle);
    uint32_t brdfIntegralSize = 512;
    auto brdfIntegralMap = rm->createTexture({
        .debugName = "brdfIntegral",
        .type = Texture::Type::t2D,
        .format = Texture::Format::r16_g16_float,
        .allStates = ResourceState::SampleSource | ResourceState::StorageComputeWrite,
        .initialState = ResourceState::StorageComputeWrite,
        .size = {brdfIntegralSize, brdfIntegralSize},
        .mipLevels = 1,
    });
    {
        Texture* brdfIntegral = rm->get(brdfIntegralMap);

        VulkanRenderer* renderer = Engine::get()->getRenderer();
        renderer->immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, integrateBRDFShader->pipeline);

                // Bind the bindless descriptor sets once per cmdbuffer
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    renderer->bindlessPipelineLayout,
                    0,
                    renderer->bindlessManager.getDescriptorSetsCount(),
                    renderer->bindlessManager.getDescriptorSets(),
                    0,
                    nullptr);

                struct IntegratePushConstants
                {
                    uint32_t outTexture;
                };
                IntegratePushConstants constants{brdfIntegral->mipResourceIndex(0)};

                // TODO: CORRECT PUSH CONSTANT RANGES FOR COMPUTE PIPELINES !!!!!
                vkCmdPushConstants(
                    cmd,
                    renderer->bindlessPipelineLayout,
                    VK_SHADER_STAGE_ALL,
                    0,
                    sizeof(IntegratePushConstants),
                    &constants);

                // TODO: dont hardcode sizes! retrieve programmatically (workrgoup size form spirv)
                vkCmdDispatch(cmd, UintDivAndCeil(brdfIntegralSize, 8), UintDivAndCeil(brdfIntegralSize, 8), 6);

                // transfer dst texture from general to shader read only layout
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = brdfIntegralMap,
                            .stateBefore = ResourceState::StorageComputeWrite,
                            .stateAfter = ResourceState::SampleSource,
                        }),
                    });
            });
    }

    // TODO: getPtrTmp() function (explicitetly state temporary!)
    auto* basicPBRMaterial = rm->get(rm->getMaterial("PBRBasic"));
    basicPBRMaterial->parameters.setResource("irradianceTex", rm->get(irradianceTexHandle)->fullResourceIndex());
    basicPBRMaterial->parameters.setResource("prefilterTex", rm->get(prefilteredEnvMap)->fullResourceIndex());
    basicPBRMaterial->parameters.setResource("brdfLUT", rm->get(brdfIntegralMap)->fullResourceIndex());
    basicPBRMaterial->parameters.pushChanges();

    auto defaultCube = rm->getMesh("DefaultCube");

    /*
        todo:
            load by default on engine init!
    */
    auto equiSkyboxMat = rm->createMaterial(
        {
            .vertexShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSky.vert"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSkyEqui.frag"},
        },
        "equiSkyboxMat");
    auto equiSkyboxMatInst = rm->createMaterialInstance(equiSkyboxMat);
    {
        auto* inst = rm->get(equiSkyboxMatInst);
        inst->parameters.setResource("equirectangularMap", rm->get(hdri)->fullResourceIndex());
        inst->parameters.pushChanges();
    }

    auto cubeSkyboxMat = rm->createMaterial(
        {
            .vertexShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSky.vert"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSkyCube.frag"},
        },
        "cubeSkyboxMat");

    auto cubeSkyboxMatInst = rm->createMaterialInstance(cubeSkyboxMat);
    {
        auto* inst = rm->get(cubeSkyboxMatInst);
        inst->parameters.setResource("cubeMap", rm->get(hdriCube)->fullResourceIndex());
        // inst->parameters.setResource("cubeMap", rm->get(irradianceTexHandle)->sampledResourceIndex);
        inst->parameters.pushChanges();
    }

    // todo: createEntity takes initial set of components as arguments and returns all the pointers as []-thingy
    auto skybox = engine.ecs.createEntity();
    {
        auto* transform = skybox.addComponent<Transform>();
        auto* renderInfo = skybox.addComponent<RenderInfo>();
        renderInfo->mesh = defaultCube;
        // renderInfo->materialInstance = equiSkyboxMatInst;
        renderInfo->materialInstance = cubeSkyboxMatInst;
    }

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