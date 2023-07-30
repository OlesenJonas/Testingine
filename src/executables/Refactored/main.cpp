#include <Engine/Engine.hpp>
#include <Engine/Graphics/Barriers/Barrier.hpp>
#include <Engine/Graphics/Texture/Sampler.hpp>
#include <Engine/Graphics/Texture/TextureToVulkan.hpp>
#include <Engine/Misc/Math.hpp>
#include <Engine/Scene/DefaultComponents.hpp>
#include <Engine/Scene/Scene.hpp>
#include <glm/gtx/transform.hpp>
#include <vulkan/vulkan_core.h>

void initScene()
{
    // Resources

    ResourceManager* rm = ResourceManager::get();
    ECS& ecs = Engine::get()->ecs;

    std::vector<glm::vec3> triangleVertexPositions;
    std::vector<Mesh::VertexAttributes> triangleVertexAttributes;
    triangleVertexPositions.resize(3);
    triangleVertexAttributes.resize(3);

    triangleVertexPositions[0] = {1.f, 1.f, 0.0f};
    triangleVertexPositions[1] = {-1.f, 1.f, 0.0f};
    triangleVertexPositions[2] = {0.f, -1.f, 0.0f};

    triangleVertexAttributes[0].color = {0.0f, 1.0f, 0.0f};
    triangleVertexAttributes[1].color = {0.0f, 1.0f, 0.0f};
    triangleVertexAttributes[2].color = {0.0f, 1.0f, 0.0f};

    auto triangleMesh = rm->createMesh(triangleVertexPositions, triangleVertexAttributes, {}, "triangle");

    auto monkeyMesh = rm->createMesh(ASSETS_PATH "/vkguide/monkey_smooth.obj", "monkey");

    auto lostEmpireMesh = ResourceManager::get()->createMesh(ASSETS_PATH "/vkguide/lost_empire.obj", "empire");

    auto lostEmpireTex = rm->createTexture(Texture::LoadInfo{
        .path = ASSETS_PATH "/vkguide/lost_empire-RGBA.png",
        .debugName = "empire_diffuse",
        .allStates = ResourceState::SampleSource,
        .initialState = ResourceState::SampleSource,
    });

    auto defaultMaterial = rm->createMaterial(
        {
            .vertexShader = {.sourcePath = SHADERS_PATH "/tri_mesh.vert"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/default_lit.frag"},
        },
        "defaultMesh");

    auto texturedMaterial = rm->createMaterial(
        {
            .vertexShader = {.sourcePath = SHADERS_PATH "/tri_mesh.vert"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/textured_lit.frag"},
        },
        "texturedMesh");

    auto solidMaterialInstance = rm->createMaterialInstance(defaultMaterial);
    auto texturedMaterialInstance = rm->createMaterialInstance(texturedMaterial);

    MaterialInstance* matInst = rm->get(texturedMaterialInstance);
    matInst->parameters.setResource("colorTexture", rm->get(lostEmpireTex)->fullResourceIndex());
    // matInst->parameters.setResource("blockySampler", rm->get(blockySampler)->resourceIndex);
    matInst->parameters.pushChanges();

    // Scene

    auto monkey = ecs.createEntity();
    {
        auto* transform = monkey.addComponent<Transform>();
        auto* renderInfo = monkey.addComponent<RenderInfo>();
        renderInfo->mesh = monkeyMesh;
        renderInfo->materialInstance = solidMaterialInstance;
        assert(renderInfo->mesh.isValid());
        assert(renderInfo->materialInstance.isValid());
    }

    for(int x = -20; x <= 20; x++)
    {
        for(int y = -20; y <= 20; y++)
        {
            glm::vec3 translation{x, 0, y};
            glm::vec3 scale{0.2f};

            auto newRenderable = ecs.createEntity();
            {
                auto* renderInfo = newRenderable.addComponent<RenderInfo>();
                renderInfo->mesh = triangleMesh;
                renderInfo->materialInstance = solidMaterialInstance;
                auto* transform = newRenderable.addComponent<Transform>();
                transform->scale = scale;
                transform->position = translation;
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
        }
    }

    auto map = ecs.createEntity();
    {
        auto* renderInfo = map.addComponent<RenderInfo>();
        renderInfo->mesh = lostEmpireMesh;
        renderInfo->materialInstance = texturedMaterialInstance;
        auto* transform = map.addComponent<Transform>();
        transform->position = glm::vec3{5.0f, -10.0f, 0.0f};
        transform->calculateLocalTransformMatrix();
        transform->localToWorld = transform->localTransform;
    }
}

int main()
{
    Engine engine;
    ResourceManager* rm = engine.getResourceManager();

    glm::quat rotAroundY = glm::quat_cast(glm::rotate(glm::radians(90.0f), glm::vec3{0.f, 1.f, 0.f}));

    // initScene();

    // not sure I like this bein called like this.
    //  would engine.loadScene(...) make more sense?
    Scene::load("C:/Users/jonas/Documents/Models/DamagedHelmet/DamagedHelmet.gltf");

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
        .allStates = ResourceState::SampleSource | ResourceState::Storage,
        .initialState = ResourceState::Undefined,
        .size = {.width = hdriCubeRes, .height = hdriCubeRes},
        .mipLevels = Texture::MipLevels::All,
    });
    Handle<ComputeShader> conversionShaderHandle =
        rm->createComputeShader({.sourcePath = SHADERS_PATH "/Skybox/equiToCube.hlsl"}, "equiToCubeCompute");
    ComputeShader* conversionShader = rm->get(conversionShaderHandle);
    struct ConversionPushConstants
    {
        uint32_t sourceIndex;
        uint32_t targetIndex;
    } constants = {
        .sourceIndex = rm->get(hdri)->fullResourceIndex(),
        .targetIndex = rm->get(hdriCube)->mipResourceIndex(0),
    };
    {
        auto* renderer = Engine::get()->getRenderer();
        renderer->immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = hdriCube,
                            .stateBefore = ResourceState::Undefined,
                            .stateAfter = ResourceState::StorageCompute,
                        }),
                    });

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
                            .stateBefore = ResourceState::StorageCompute,
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
        .allStates = ResourceState::SampleSource | ResourceState::Storage,
        .initialState = ResourceState::Undefined,
        .size = {irradianceRes, irradianceRes, 1},
    });
    {
        Texture* irradianceTex = rm->get(irradianceTexHandle);

        VulkanRenderer* renderer = Engine::get()->getRenderer();
        renderer->immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                // transition dst texture to compute storage state
                submitBarriers(
                    cmd,
                    {
                        Barrier::from(Barrier::Image{
                            .texture = irradianceTexHandle,
                            .stateBefore = ResourceState::Undefined,
                            .stateAfter = ResourceState::StorageCompute,
                        }),
                    });

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
                            .stateBefore = ResourceState::StorageCompute,
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
        .allStates = ResourceState::SampleSource | ResourceState::Storage,
        .initialState = ResourceState::Storage,
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
                            .stateBefore = ResourceState::StorageCompute,
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
        .allStates = ResourceState::SampleSource | ResourceState::Storage,
        .initialState = ResourceState::Storage,
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
                            .stateBefore = ResourceState::StorageCompute,
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

    while(engine.isRunning())
    {
        // will probably split when accessing specific parts of the update is needed
        // but thats not the case atm
        engine.update();
    }

    return 0;
}