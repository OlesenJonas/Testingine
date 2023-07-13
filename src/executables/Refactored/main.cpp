#include <Engine/Engine.hpp>
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

    auto lostEmpireTex = rm->createTexture(
        ASSETS_PATH "/vkguide/lost_empire-RGBA.png", VK_IMAGE_USAGE_SAMPLED_BIT, false, "empire_diffuse");

    auto linearSampler = rm->createSampler({
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    });
    auto blockySampler = rm->createSampler({
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
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
    matInst->parameters.setResource("colorTexture", rm->get(lostEmpireTex)->sampledResourceIndex);
    matInst->parameters.setResource("blockySampler", rm->get(blockySampler)->resourceIndex);
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

    auto linearSampler = rm->createSampler({
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    });

    auto hdri = engine.getResourceManager()->createTexture(
        ASSETS_PATH "/HDRIs/kloppenheim_04_2k.hdr", VK_IMAGE_USAGE_SAMPLED_BIT, true);
    auto hdriCube = rm->createCubemapFromEquirectangular(512, hdri, "HdriCubemap");

    Handle<ComputeShader> calcIrradianceComp = rm->createComputeShader(
        {.sourcePath = SHADERS_PATH "/Skybox/generateIrradiance.comp"}, "generateIrradiance");
    ComputeShader* calcIrradianceShader = rm->get(calcIrradianceComp);

    uint32_t irradianceRes = 32;
    auto irradianceTexHandle = rm->createTexture({
        .debugName = "hdriIrradiance",
        .size = {irradianceRes, irradianceRes, 1},
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .arrayLayers = 6,
    });
    {
        Texture* irradianceTex = rm->get(irradianceTexHandle);

        // generate the irradiance
        //   TODO: factor out stuff from here, shouldnt be this much code
        //         esp barriers should be abstracted at least some what

        VulkanRenderer* renderer = Engine::get()->getRenderer();
        renderer->immediateSubmit(
            [=](VkCommandBuffer cmd)
            {
                ResourceManager* rm = Engine::get()->getResourceManager();

                // transfer dst texture to general layout
                VkImageMemoryBarrier2 imgMemoryBarrier{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .pNext = nullptr,

                    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

                    .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,

                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,

                    .srcQueueFamilyIndex = renderer->graphicsAndComputeQueueFamily,
                    .dstQueueFamilyIndex = renderer->graphicsAndComputeQueueFamily,

                    .image = irradianceTex->image,
                    .subresourceRange =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = irradianceTex->info.arrayLayers,
                        },
                };

                VkDependencyInfo dependInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .pNext = nullptr,
                    .dependencyFlags = 0,
                    .memoryBarrierCount = 0,
                    .bufferMemoryBarrierCount = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = &imgMemoryBarrier,
                };

                vkCmdPipelineBarrier2(cmd, &dependInfo);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, calcIrradianceShader->pipeline);
                // Bind the bindless descriptor sets once per cmdbuffer
                vkCmdBindDescriptorSets(
                    cmd,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    renderer->bindlessPipelineLayout,
                    0,
                    4,
                    renderer->bindlessManager.bindlessDescriptorSets.data(),
                    0,
                    nullptr);

                auto cubeSampler = rm->createSampler({
                    .magFilter = VK_FILTER_LINEAR,
                    .minFilter = VK_FILTER_LINEAR,
                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                });

                struct ConversionPushConstants
                {
                    uint32_t sourceIndex;
                    uint32_t samplerIndex;
                    uint32_t targetIndex;
                };
                ConversionPushConstants constants{
                    .sourceIndex = rm->get(hdriCube)->sampledResourceIndex,
                    .samplerIndex = rm->get(cubeSampler)->resourceIndex,
                    .targetIndex = irradianceTex->storageResourceIndex,
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
                imgMemoryBarrier = VkImageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .pNext = nullptr,

                    .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,

                    .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,

                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

                    .srcQueueFamilyIndex = renderer->graphicsAndComputeQueueFamily,
                    .dstQueueFamilyIndex = renderer->graphicsAndComputeQueueFamily,

                    .image = irradianceTex->image,
                    .subresourceRange =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = irradianceTex->info.arrayLayers,
                        },
                };

                dependInfo = VkDependencyInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .pNext = nullptr,
                    .dependencyFlags = 0,
                    .memoryBarrierCount = 0,
                    .bufferMemoryBarrierCount = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = &imgMemoryBarrier,
                };

                vkCmdPipelineBarrier2(cmd, &dependInfo);
            });
    }

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
        inst->parameters.setResource("equirectangularMap", rm->get(hdri)->sampledResourceIndex);
        inst->parameters.setResource("defaultSampler", rm->get(linearSampler)->resourceIndex);
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
        // inst->parameters.setResource("cubeMap", rm->get(hdriCube)->sampledResourceIndex);
        inst->parameters.setResource("cubeMap", rm->get(irradianceTexHandle)->sampledResourceIndex);
        inst->parameters.setResource("defaultSampler", rm->get(linearSampler)->resourceIndex);
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