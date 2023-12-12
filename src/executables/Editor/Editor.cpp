#include "Editor.hpp"
#include "Scene/DefaultComponents.hpp"
#include "Scene/Scene.hpp"
#include <Datastructures/Span.hpp>
#include <Engine/Graphics/Barrier/Barrier.hpp>
#include <Engine/Graphics/Mesh/Cube.hpp>
#include <Engine/Graphics/Mesh/Fullscreen.hpp>
#include <Engine/Misc/Math.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <format>
#include <fstream>
#include <future>

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

Editor::Editor()
    : Application(Application::CreateInfo{
          .name = "Testingine Editor",
          .windowHints = {{GLFW_MAXIMIZED, GLFW_TRUE}},
      })
{
    ZoneScopedN("Editor Init");
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

    mainCamera =
        Camera{static_cast<float>(mainWindow.width) / static_cast<float>(mainWindow.height), 0.1f, 1000.0f};

    createDefaultAssets();

    // Disable validation error breakpoints during init, synch errors arent correct
    gfxDevice.disableValidationErrorBreakpoint();

    // TODO: execute this while GPU is already doing work, instead of waiting for this *then* starting GPU
    Scene::load("C:/Users/jonas/Documents/Models/Sponza/out/Sponza.gltf", &ecs, scene.root);

    // ------------------------ Build MeshData & InstanceInfo buffer ------------------------------------------
    TracyCZoneN(zoneGPUScene, "Build GPU Scene", true);
    VkCommandBuffer mainCmdBuffer = gfxDevice.beginCommandBuffer();

    auto& rm = resourceManager;

    Buffer::Handle meshDataAllocBuffer = resourceManager.createBuffer(Buffer::CreateInfo{
        .debugName = "tempMeshDataBuffer",
        .size = sizeof(GPUMeshData) * resourceManager.getMeshPool().size(),
        .memoryType = Buffer::MemoryType::CPU,
        .allStates = ResourceState::TransferSrc,
        .initialState = ResourceState::TransferSrc,
    });
    // not sure how good assigning single GPUObjectDatas is (vs CPU buffer and then one memcpy)
    auto* gpuPtr = (GPUMeshData*)(*rm.get<void*>(meshDataAllocBuffer));
    // fill MeshData buffer with all meshes
    //      TODO: not all, just the ones being used in scene!
    const auto& meshPool = rm.getMeshPool();
    for(auto iter = meshPool.begin(); iter != meshPool.end(); iter++)
    {
        Mesh::Handle mesh = *iter;

        // TODO: const correctness
        Mesh::RenderDataCPU& renderData = *resourceManager.get<Mesh::RenderDataCPU>(mesh);

        if(renderData.meshletCount == 0 || !renderData.positionBuffer.isNonNull())
        {
            break;
        }

        auto freeIndex = gpuMeshDataBuffer.freeIndex++;
        assert(renderData.gpuIndex == 0xFFFFFFFF);
        renderData.gpuIndex = freeIndex;
        gpuPtr[freeIndex] = GPUMeshData{
            // .indexCount = renderData.indexCount,
            .meshletCount = renderData.meshletCount,
            .additionalUVCount = renderData.additionalUVCount,
            // .indexBuffer = *rm.get<ResourceIndex>(renderData.indexBuffer),
            .positionBuffer = *rm.get<ResourceIndex>(renderData.positionBuffer),
            .attributeBuffer = *rm.get<ResourceIndex>(renderData.attributeBuffer),
            .meshletVertexIndices = *rm.get<ResourceIndex>(renderData.meshletVertices),
            .meshletPrimitiveIndices = *rm.get<ResourceIndex>(renderData.meshletPrimitiveIndices),
            .meshletDescriptors = *rm.get<ResourceIndex>(renderData.meshletDescriptors),
        };
        assert(resourceManager.get<Mesh::RenderDataCPU>(mesh)->gpuIndex != 0xFFFFFFFF);
    }
    gfxDevice.copyBuffer(mainCmdBuffer, meshDataAllocBuffer, gpuMeshDataBuffer.buffer);
    gfxDevice.destroy(meshDataAllocBuffer);

    // create instance info for each object in scene
    Buffer::Handle instanceAllocBuffer = resourceManager.createBuffer(Buffer::CreateInfo{
        .debugName = "tempInstanceInfoBuffer",
        .size = sizeof(InstanceInfo) * ecs.count<MeshRenderer, Transform>() * Mesh::MAX_SUBMESHES,
        .memoryType = Buffer::MemoryType::CPU,
        .allStates = ResourceState::TransferSrc,
        .initialState = ResourceState::TransferSrc,
    });
    auto* gpuInstancePtr = (InstanceInfo*)(*rm.get<void*>(instanceAllocBuffer));
    ecs.forEach<MeshRenderer, Transform>(
        [&](MeshRenderer* meshRenderer, Transform* transform)
        {
            for(int i = 0; i < Mesh::MAX_SUBMESHES; i++)
            {
                assert(meshRenderer->instanceBufferIndices[i] == 0xFFFFFFFF);

                const Mesh::Handle mesh = meshRenderer->subMeshes[i];
                if(!mesh.isNonNull())
                {
                    break;
                }
                assert(resourceManager.getMeshPool().isHandleValid(mesh));
                const Mesh::RenderData& renderData = *rm.get<Mesh::RenderData>(mesh);
                assert(renderData.gpuIndex != 0xFFFFFFFF);

                auto* name = rm.get<std::string>(mesh);

                auto freeIndex = gpuInstanceInfoBuffer.freeIndex++;
                const MaterialInstance::Handle matInst = meshRenderer->materialInstances[i];
                const Buffer::Handle matInstParamBuffer = rm.get<Material::ParameterBuffer>(matInst)->deviceBuffer;
                bool hasMatInstParameters = matInstParamBuffer.isNonNull();
                const Material::Handle mat = *rm.get<Material::Handle>(matInst);
                const Buffer::Handle matParamBuffer = rm.get<Material::ParameterBuffer>(mat)->deviceBuffer;
                bool hasMatParameters = matParamBuffer.isNonNull();

                gpuInstancePtr[freeIndex] = InstanceInfo{
                    .transform = transform->localToWorld,
                    .invTranspTransform = glm::inverseTranspose(transform->localToWorld),
                    .meshDataIndex = renderData.gpuIndex,
                    .materialIndex = 0xFFFFFFFF, // TODO: correct value
                    .materialParamsBuffer = hasMatParameters ? *rm.get<ResourceIndex>(matParamBuffer) : 0xFFFFFFFF,
                    .materialInstanceParamsBuffer =
                        hasMatInstParameters ? *rm.get<ResourceIndex>(matInstParamBuffer) : 0xFFFFFFFF,
                };
                meshRenderer->instanceBufferIndices[i] = freeIndex;
            }
        });
    gfxDevice.copyBuffer(mainCmdBuffer, instanceAllocBuffer, gpuInstanceInfoBuffer.buffer);
    gfxDevice.destroy(instanceAllocBuffer);
    TracyCZoneEnd(zoneGPUScene);

    // ------------------------------------------------------------------------------

    gfxDevice.enableValidationErrorBreakpoint();

    gfxDevice.endCommandBuffer(mainCmdBuffer);

    VkCommandBuffer materialUpdateCmds = updateDirtyMaterialParameters();

    TracyCZoneN(zoneWaitGPU, "Waiting for GPU submit", true);
    // TODO: submit inbetween with work that can be submitted already!
    //       ie: do stuff like generating irradiance etc while loading glTF scene!
    gfxDevice.submitInitializationWork({materialUpdateCmds, mainCmdBuffer});

    // just to be safe, wait for all commands to be done here
    gfxDevice.waitForWorkFinished();
    TracyCZoneEnd(zoneWaitGPU);

    gfxDevice.enableValidationErrorBreakpoint();

    glfwSetTime(0.0);
    inputManager.resetTime();
}

void Editor::createDefaultAssets()
{
    // create this first, need to ensure resourceIndex is 0 (since thats currently hardcoded in the shaders)
    //  TODO: switch to spec constant?
    //  TODO: alternatively add "reserveBinding" functionality and add request for given number in CreateInfo!
    gpuMeshDataBuffer.buffer = resourceManager.createBuffer(Buffer::CreateInfo{
        .debugName = "MeshDataBuffer",
        .size = sizeof(GPUMeshData) * gpuMeshDataBuffer.limit,
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::Storage | ResourceState::TransferDst,
        .initialState = ResourceState::TransferDst,
    });
    assert(*resourceManager.get<ResourceIndex>(gpuMeshDataBuffer.buffer) == 0);

    gpuInstanceInfoBuffer.buffer = resourceManager.createBuffer(Buffer::CreateInfo{
        .debugName = "instanceInfoBuffer",
        .size = sizeof(InstanceInfo) * gpuInstanceInfoBuffer.limit,
        .memoryType = Buffer::MemoryType::GPU,
        .allStates = ResourceState::Storage | ResourceState::TransferDst,
        .initialState = ResourceState::TransferDst,
    });

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

    createDefaultSamplers();

    createDefaultComputeShaders();

    createDefaultTextures();

    SkyboxTextures skyboxTextures = generateSkyboxTextures(512, 32, 128);

    createDefaultMeshes();

    createRendertargets();

    createDefaultMaterialAndInstances();

    Material::Handle pbrMat = resourceManager.getMaterial("PBRBasic");
    Material::setResource(
        pbrMat, "irradianceTex", *resourceManager.get<ResourceIndex>(skyboxTextures.irradianceMap));
    Material::setResource(
        pbrMat, "prefilterTex", *resourceManager.get<ResourceIndex>(skyboxTextures.prefilteredMap));
    Material::setResource(pbrMat, "brdfLUT", *resourceManager.get<ResourceIndex>(brdfIntegralTex));

    MaterialInstance::setResource(unlitMatInst, "texture", *resourceManager.get<ResourceIndex>(mipDebugTex));

    MaterialInstance::setResource(
        equiSkyboxMatInst, "equirectangularMap", *resourceManager.get<ResourceIndex>(defaultHDRI));

    MaterialInstance::setResource(
        cubeSkyboxMatInst, "cubeMap", *resourceManager.get<ResourceIndex>(skyboxTextures.cubeMap));

    auto skybox = scene.createEntity();
    {
        auto* meshRenderer = skybox.addComponent<MeshRenderer>();
        meshRenderer->subMeshes[0] = resourceManager.getMesh("DefaultCube");
        assert(!meshRenderer->subMeshes[1].isNonNull());
        // renderInfo->materialInstance = equiSkyboxMatInst;
        meshRenderer->materialInstances[0] = cubeSkyboxMatInst;
    }

    auto triangleObject = scene.createEntity();
    {
        auto* meshRenderer = triangleObject.addComponent<MeshRenderer>();
        meshRenderer->subMeshes[0] = triangleMesh;
        meshRenderer->materialInstances[0] = unlitMatInst;
        auto* transform = triangleObject.getComponent<Transform>();
        transform->position = glm::vec3{3.0f, 0.0f, 0.0f};
        transform->calculateLocalTransformMatrix();
        // dont like having to call this manually
        transform->localToWorld = transform->localTransform;

        assert(meshRenderer->subMeshes[0].isNonNull());
        assert(meshRenderer->materialInstances[0].isNonNull());
    }
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
        SHADERS_PATH "/includes/Bindless/DefaultSamplers.hlsl", std::ofstream::out | std::ofstream::trunc);
    for(auto& info : defaultInfos)
    {
        defaultSamplerDefines << std::format("#define {} {}\n", info.name, info.index);
    }
    defaultSamplerDefines.close();
}

void Editor::createDefaultMeshes()
{
    resourceManager.createMesh(
        Cube::positions,
        Cube::attributes(),
        Mesh::VertexAttributeFormat{.additionalUVCount = 0},
        Cube::indices,
        "DefaultCube");
    assert(resourceManager.get<std::string>(resourceManager.getMesh("DefaultCube")) != nullptr);

    std::vector<glm::vec3> triangleVertexPositions;
    std::vector<Mesh::BasicVertexAttributes<0>> triangleVertexAttributes;
    triangleVertexPositions.resize(3);
    triangleVertexAttributes.resize(3);

    triangleVertexPositions[0] = {1.f, 1.f, 0.0f};
    triangleVertexPositions[1] = {-1.f, 1.f, 0.0f};
    triangleVertexPositions[2] = {0.f, -1.f, 0.0f};

    triangleVertexAttributes[0].uvs[0] = {1.0f, 0.0f};
    triangleVertexAttributes[1].uvs[0] = {0.0f, 0.0f};
    triangleVertexAttributes[2].uvs[0] = {0.5f, 1.0f};

    Span<std::byte> attribsAsBytes{
        (std::byte*)triangleVertexAttributes.data(),
        triangleVertexAttributes.size() * sizeof(triangleVertexAttributes[0])};
    triangleMesh = resourceManager.createMesh(
        triangleVertexPositions,
        attribsAsBytes,
        Mesh::VertexAttributeFormat{.additionalUVCount = 0},
        {},
        "triangle");
}

void Editor::createDefaultMaterialAndInstances()
{
    ZoneScopedN("Create Materials and Instances");

    std::vector<Material::Handle> materials = resourceManager.createMaterials({
        {
            .debugName = "writeToSwapchain",
            .vertexShader = {.sourcePath = SHADERS_PATH "/WriteToSwapchain/WriteToSwapchain.vert"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/WriteToSwapchain/WriteToSwapchain.frag"},
            .colorFormats = {Texture::Format::B8_G8_R8_A8_SRGB},
        },
        {
            // TODO: change back to mesh!
            .debugName = "equiSkyboxMat",
            .meshShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSky.mesh"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSkyEqui.frag"},
            .colorFormats = {offscreenRTFormat},
            .depthFormat = depthFormat,
        },
        {
            .debugName = "cubeSkyboxMat",
            .meshShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSky.mesh"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/Skybox/hdrSkyCube.frag"},
            .colorFormats = {offscreenRTFormat},
            .depthFormat = depthFormat,
        },
        {
            .debugName = "PBRBasic",
            .meshShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasic.mesh"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasic.frag"},
            .colorFormats = {offscreenRTFormat},
            .depthFormat = depthFormat,
        },
        {
            .debugName = "texturedUnlit",
            .meshShader = {.sourcePath = SHADERS_PATH "/Unlit/TexturedUnlit.mesh"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/Unlit/TexturedUnlit.frag"},
            .colorFormats = {offscreenRTFormat},
            .depthFormat = depthFormat,
        },
    });
    writeToSwapchainMat = materials[0];
    Material::Handle equiSkyboxMat = materials[1];
    Material::Handle cubeSkyboxMat = materials[2];
    Material::Handle pbrBasicMat = materials[3];
    Material::Handle unlitTexturedMat = materials[4];

    equiSkyboxMatInst = resourceManager.createMaterialInstance(equiSkyboxMat);
    cubeSkyboxMatInst = resourceManager.createMaterialInstance(cubeSkyboxMat);
    unlitMatInst = resourceManager.createMaterialInstance(unlitTexturedMat);
}

void Editor::createDefaultComputeShaders()
{
    ZoneScopedN("Compile compute shaders");

    std::vector<Handle<ComputeShader>> shaders = resourceManager.createComputeShaders({
        {.sourcePath = SHADERS_PATH "/Skybox/equiToCube.hlsl", .debugName = "equiToCubeCompute"},
        {.sourcePath = SHADERS_PATH "/Skybox/generateIrradiance.comp", .debugName = "generateIrradiance"},
        {.sourcePath = SHADERS_PATH "/Misc/debugMipFill.comp", .debugName = "debugMipFill"},
        {.sourcePath = SHADERS_PATH "/Skybox/prefilter.comp", .debugName = "prefilterEnvComp"},
        {.sourcePath = SHADERS_PATH "/PBR/integrateBRDF.comp", .debugName = "integrateBRDFComp"} //
    });
    equiToCubeShader = shaders[0];
    irradianceCalcShader = shaders[1];
    debugMipFillShader = shaders[2];
    prefilterEnvShader = shaders[3];
    integrateBrdfShader = shaders[4];
}

void Editor::createRendertargets()
{
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
}

void Editor::createDefaultTextures()
{
    ZoneScopedN("Create default textures");

    auto& gfxDevice = *VulkanDevice::impl();
    VkCommandBuffer cmd = gfxDevice.beginCommandBuffer();

    mipDebugTex = resourceManager.createTexture({
        .debugName = "mipTest",
        .type = Texture::Type::t2D,
        .format = Texture::Format::R8_G8_B8_A8_UNORM,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {128, 128},
        .mipLevels = Texture::MipLevels::All,
        .cmdBuf = cmd,
    });
    ComputeShader* debugMipFillShader = resourceManager.get(this->debugMipFillShader);
    {
        gfxDevice.startDebugRegion(cmd, "Mip test tex filling");
        gfxDevice.setComputePipelineState(cmd, debugMipFillShader->pipeline);

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

    ComputeShader* integrateBRDFShader = resourceManager.get(this->integrateBrdfShader);
    uint32_t brdfIntegralSize = 512;
    brdfIntegralTex = resourceManager.createTexture({
        .debugName = "brdfIntegral",
        .type = Texture::Type::t2D,
        .format = Texture::Format::R16_G16_FLOAT,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {brdfIntegralSize, brdfIntegralSize},
        .mipLevels = 1,
        .cmdBuf = cmd,
    });
    {
        gfxDevice.setComputePipelineState(cmd, integrateBRDFShader->pipeline);

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

    gfxDevice.endCommandBuffer(cmd);
    gfxDevice.simpleSubmit(cmd);
}

Editor::SkyboxTextures
Editor::generateSkyboxTextures(uint32_t hdriCubeRes, uint32_t irradianceRes, uint32_t prefilteredEnvMapBaseSize)
{
    auto& gfxDevice = *VulkanDevice::impl();
    VkCommandBuffer cmd = gfxDevice.beginCommandBuffer();

    TracyCZoneN(zoneSkyHDRI, "Load Sky HDRI", true);
    defaultHDRI = resourceManager.createTexture(Texture::LoadInfo{
        // .path = ASSETS_PATH "/HDRIs/kloppenheim_04_2k.hdr",
        .path = "C:/Users/jonas/Documents/Models/Bistro/san_giuseppe_bridge_4k.hdr",
        .fileDataIsLinear = true,
        .allStates = ResourceState::SampleSource,
        .initialState = ResourceState::SampleSource,
        .cmdBuf = cmd,
    });
    TracyCZoneEnd(zoneSkyHDRI);

    std::string baseName = *resourceManager.get<std::string>(defaultHDRI);

    auto hdriCube = resourceManager.createTexture(Texture::CreateInfo{
        // specifying non-default values only
        .debugName = baseName + "Cubemap",
        .type = Texture::Type::tCube,
        .format = resourceManager.get<Texture::Descriptor>(defaultHDRI)->format,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {.width = hdriCubeRes, .height = hdriCubeRes},
        .mipLevels = Texture::MipLevels::All,
        .cmdBuf = cmd,
    });
    {
        ComputeShader* conversionShader = resourceManager.get(equiToCubeShader);

        gfxDevice.setComputePipelineState(cmd, resourceManager.get(equiToCubeShader)->pipeline);

        struct ConversionPushConstants
        {
            uint32_t sourceIndex;
            uint32_t targetIndex;
        } constants = {
            .sourceIndex = *resourceManager.get<ResourceIndex>(defaultHDRI),
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
        .cmdBuf = cmd,
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

    ComputeShader* prefilterEnvShader = resourceManager.get(this->prefilterEnvShader);

    auto prefilteredEnvMap = resourceManager.createTexture({
        .debugName = baseName + "Prefiltered",
        .type = Texture::Type::tCube,
        .format = Texture::Format::R16_G16_B16_A16_FLOAT,
        .allStates = ResourceState::SampleSource | ResourceState::StorageCompute,
        .initialState = ResourceState::StorageCompute,
        .size = {prefilteredEnvMapBaseSize, prefilteredEnvMapBaseSize},
        .mipLevels = 5,
        .cmdBuf = cmd,
    });
    {
        gfxDevice.setComputePipelineState(cmd, prefilterEnvShader->pipeline);

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

    gfxDevice.endCommandBuffer(cmd);
    gfxDevice.simpleSubmit(cmd);

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
    ZoneScoped;
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

    if(ImGui::IsKeyPressed(ImGuiKey_R, false))
    {
        Material::Handle pbrMat = resourceManager.getMaterial("PBRBasic");
        assert(pbrMat.isNonNull());
        resourceManager.reloadMaterial(pbrMat);
    }

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
    FrameMark;
}

VkCommandBuffer Editor::drawScene(int threadIndex)
{
    ZoneScoped;
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

    GraphicsPushConstants pushConstants;
    pushConstants.renderInfoBuffer =
        *resourceManager.get<ResourceIndex>(getCurrentFrameData().renderPassDataBuffer);
    pushConstants.instanceBuffer = *resourceManager.get<ResourceIndex>(gpuInstanceInfoBuffer.buffer);

    gfxDevice.pushConstants(offscreenCmdBuffer, sizeof(GraphicsPushConstants), &pushConstants);

    Mesh::Handle lastMesh = Mesh::Handle::Invalid();
    Material::Handle lastMaterial = Material::Handle::Invalid();
    MaterialInstance::Handle lastMaterialInstance = MaterialInstance::Handle::Invalid();
    uint32_t meshletCount = 0;

    ecs.forEach<MeshRenderer>(
        [&](MeshRenderer* meshRenderer)
        {
            for(int subMeshIndx = 0; subMeshIndx < Mesh::MAX_SUBMESHES; subMeshIndx++)
            {
                Mesh::Handle objectMesh = meshRenderer->subMeshes[subMeshIndx];
                if(!objectMesh.isNonNull())
                {
                    break;
                }

                MaterialInstance::Handle objectMaterialInstance = meshRenderer->materialInstances[subMeshIndx];

                if(objectMaterialInstance != lastMaterialInstance)
                {
                    Material::Handle newMaterial = *resourceManager.get<Material::Handle>(objectMaterialInstance);
                    if(newMaterial != lastMaterial)
                    {
                        gfxDevice.setGraphicsPipelineState(
                            offscreenCmdBuffer, *resourceManager.get<VkPipeline>(newMaterial));
                        lastMaterial = newMaterial;
                    }
                }

                if(objectMesh != lastMesh)
                {
                    const Mesh::RenderDataCPU& meshData = *resourceManager.get<Mesh::RenderDataCPU>(objectMesh);
                    meshletCount = meshData.meshletCount;
                    lastMesh = objectMesh;
                }

                // gfxDevice.draw(offscreenCmdBuffer, indexCount, 1, 0,
                // meshRenderer->instanceBufferIndices[subMeshIndx]);
                pushConstants.indexInInstanceBuffer = meshRenderer->instanceBufferIndices[subMeshIndx];
                gfxDevice.pushConstants(offscreenCmdBuffer, sizeof(GraphicsPushConstants), &pushConstants);
                gfxDevice.drawMeshlets(offscreenCmdBuffer, meshletCount);
            }
        });

    gfxDevice.endRendering(offscreenCmdBuffer);
    gfxDevice.endCommandBuffer(offscreenCmdBuffer);

    return offscreenCmdBuffer;
};

VkCommandBuffer Editor::drawUI(int threadIndex)
{
    ZoneScoped;
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

    Material::Handle material = writeToSwapchainMat;
    gfxDevice.setGraphicsPipelineState(onscreenCmdBuffer, *resourceManager.get<VkPipeline>(material));
    ResourceIndex inputTex = *resourceManager.get<ResourceIndex>(offscreenTexture);
    gfxDevice.pushConstants(onscreenCmdBuffer, sizeof(ResourceIndex), &inputTex);

    gfxDevice.draw(onscreenCmdBuffer, 3, 1, 0, 0);

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
        memcpy(gpuAlloc.ptr, paramBuffer.cpuBuffer, paramBuffer.size);
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
        memcpy(gpuAlloc.ptr, paramBuffer.cpuBuffer, paramBuffer.size);
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