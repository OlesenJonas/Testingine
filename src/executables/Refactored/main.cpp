#include <glm/gtx/transform.hpp>
#include <intern/Engine/Engine.hpp>
#include <intern/Graphics/Renderer/VulkanRenderer.hpp>

void initMeshes()
{
    std::vector<Vertex> triangleVertices;
    triangleVertices.resize(3);

    triangleVertices[0].position = {1.f, 1.f, 0.0f};
    triangleVertices[1].position = {-1.f, 1.f, 0.0f};
    triangleVertices[2].position = {0.f, -1.f, 0.0f};

    triangleVertices[0].color = {0.0f, 1.0f, 0.0f};
    triangleVertices[1].color = {0.0f, 1.0f, 0.0f};
    triangleVertices[2].color = {0.0f, 1.0f, 0.0f};

    auto triangleMesh = Engine::get()->getResourceManager()->createMesh(triangleVertices, "triangle");

    // load monkey mesh
    auto monkeyMesh =
        Engine::get()->getResourceManager()->createMesh(ASSETS_PATH "/vkguide/monkey_smooth.obj", "monkey");

    auto lostEmpire =
        Engine::get()->getResourceManager()->createMesh(ASSETS_PATH "/vkguide/lost_empire.obj", "empire");
}

void initScene()
{
    auto& rsrcManager = *Engine::get()->getResourceManager();
    auto& renderables = Engine::get()->getRenderer()->renderables;

    const auto& newRenderable = renderables.emplace_back(RenderObject{
        .mesh = rsrcManager.getMesh("monkey"),
        .material = rsrcManager.getMaterial("defaultMesh"),
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
                .mesh = rsrcManager.getMesh("triangle"),
                .material = rsrcManager.getMaterial("defaultMesh"),
                .transformMatrix = translation * scale,
            });
            assert(newRenderable.mesh.isValid());
            assert(newRenderable.material.isValid());
        }
    }

    RenderObject map;
    map.mesh = rsrcManager.getMesh("empire");
    map.material = rsrcManager.getMaterial("texturedMesh");
    map.transformMatrix = glm::translate(glm::vec3{5.0f, -10.0f, 0.0f});
    renderables.push_back(map);
}

int main()
{
    Engine engine;

    VulkanRenderer& renderer = *engine.getRenderer();

    initMeshes();
    initScene();

    while(engine.isRunning())
    {
        // will probably split when accessing specific parts of the update is needed
        // but thats not the case atm
        engine.update();
    }

    return 0;
}