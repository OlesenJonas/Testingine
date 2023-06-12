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
        ASSETS_PATH "/vkguide/lost_empire-RGBA.png", VK_IMAGE_USAGE_SAMPLED_BIT, "empire_diffuse");

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
        monkey.addComponent<Transform>();
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
    }
}

int main()
{
    Engine engine;

    glm::quat rotAroundY = glm::quat_cast(glm::rotate(glm::radians(90.0f), glm::vec3{0.f, 1.f, 0.f}));

    // initScene();

    // not sure I like this bein called like this.
    //  would engine.loadScene(...) make more sense?
    Scene::load("C:/Users/jonas/Documents/Models/NormalTestglTF/NormalTangentTest.gltf");

    while(engine.isRunning())
    {
        // will probably split when accessing specific parts of the update is needed
        // but thats not the case atm
        engine.update();
    }

    return 0;
}