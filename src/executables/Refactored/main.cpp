#include "intern/ResourceManager/ResourceManager.hpp"
#include <glm/gtx/transform.hpp>
#include <intern/Engine/Engine.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>

void initResources()
{
    ResourceManager* rm = ResourceManager::get();

    std::vector<Vertex> triangleVertices;
    triangleVertices.resize(3);

    triangleVertices[0].position = {1.f, 1.f, 0.0f};
    triangleVertices[1].position = {-1.f, 1.f, 0.0f};
    triangleVertices[2].position = {0.f, -1.f, 0.0f};

    triangleVertices[0].color = {0.0f, 1.0f, 0.0f};
    triangleVertices[1].color = {0.0f, 1.0f, 0.0f};
    triangleVertices[2].color = {0.0f, 1.0f, 0.0f};

    auto triangleMesh = rm->createMesh(triangleVertices, "triangle");

    auto monkeyMesh = rm->createMesh(ASSETS_PATH "/vkguide/monkey_smooth.obj", "monkey");

    auto lostEmpireMesh = ResourceManager::get()->createMesh(ASSETS_PATH "/vkguide/lost_empire.obj", "empire");

    auto lostEmpireTex = rm->createTexture(
        ASSETS_PATH "/vkguide/lost_empire-RGBA.png", VK_IMAGE_USAGE_SAMPLED_BIT, "empire_diffuse");

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

    Material* mat = rm->get(texturedMaterial);
    mat->setResource("colorTexture", rm->get(lostEmpireTex)->sampledResourceIndex);
    mat->pushParameterChanges();
}

void initScene()
{
    initResources();

    ResourceManager* rm = ResourceManager::get();

    auto& renderables = VulkanRenderer::get()->renderables;

    const auto& newRenderable = renderables.emplace_back(RenderObject{
        .mesh = rm->getMesh("monkey"),
        .material = rm->getMaterial("defaultMesh"),
        .transformMatrix = glm::mat4{1.0f},
    });
    assert(newRenderable.mesh.isValid());
    assert(newRenderable.material.isValid());

    for(int x = -20; x <= 20; x++)
    {
        for(int y = -20; y <= 20; y++)
        {
            glm::mat4 translation = glm::translate(glm::vec3{x, 0, y});
            glm::mat4 scale = glm::scale(glm::vec3{0.2f});

            const auto& newRenderable = renderables.emplace_back(RenderObject{
                .mesh = rm->getMesh("triangle"),
                .material = rm->getMaterial("defaultMesh"),
                .transformMatrix = translation * scale,
            });
            assert(newRenderable.mesh.isValid());
            assert(newRenderable.material.isValid());
        }
    }

    RenderObject map;
    map.mesh = rm->getMesh("empire");
    map.material = rm->getMaterial("texturedMesh");
    map.transformMatrix = glm::translate(glm::vec3{5.0f, -10.0f, 0.0f});
    renderables.push_back(map);
}

int main()
{
    Engine engine;

    VulkanRenderer& renderer = *engine.getRenderer();

    initScene();

    while(engine.isRunning())
    {
        // will probably split when accessing specific parts of the update is needed
        // but thats not the case atm
        engine.update();
    }

    return 0;
}