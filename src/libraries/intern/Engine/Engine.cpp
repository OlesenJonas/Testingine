#include "Engine.hpp"
#include <Engine/Scene/DefaultComponents.hpp>
#include <Graphics/Mesh/Cube.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>

Engine::Engine()
    : mainWindow(1200, 800, "Vulkan Test", {{GLFW_MAXIMIZED, GLFW_TRUE}}), sceneRoot(ecs.createEntity())
{
    ptr = this;

    // This needs to come first here, otherwise these callbacks will override
    // ImGuis callbacks set-up in renderer.init()
    inputManager.init();

    resourceManager.init();
    renderer.init();

    ecs.registerComponent<Transform>();
    ecs.registerComponent<Hierarchy>();
    ecs.registerComponent<RenderInfo>();

    sceneRoot.addComponent<Transform>();
    sceneRoot.addComponent<Hierarchy>();

    // TODO: remove completly once everything works
    // default resources
    // resourceManager.createMaterial(
    //     {
    //         .vertexShader =
    //             {.sourcePath = SHADERS_PATH "/PBRBasic.vert", .sourceLanguage = Shaders::Language::GLSL},
    //         .fragmentShader =
    //             {.sourcePath = SHADERS_PATH "/PBRBasic.frag", .sourceLanguage = Shaders::Language::GLSL},
    //     },
    //     "PBRBasic");
    // assert(resourceManager.get(resourceManager.getMaterial("PBRBasic")) != nullptr);

    resourceManager.createMaterial(
        {
            .vertexShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasicVert.hlsl"},
            .fragmentShader = {.sourcePath = SHADERS_PATH "/PBR/PBRBasicFrag.hlsl"},
        },
        "PBRBasic");
    assert(resourceManager.get(resourceManager.getMaterial("PBRBasic")) != nullptr);

    resourceManager.createMesh(Cube::positions, Cube::attributes, Cube::indices, "DefaultCube");
    assert(resourceManager.get(resourceManager.getMesh("DefaultCube")) != nullptr);

    mainCamera =
        Camera{static_cast<float>(mainWindow.width) / static_cast<float>(mainWindow.height), 0.1f, 1000.0f};

    glfwSetTime(0.0);
    Engine::get()->getInputManager()->resetTime();
}

Engine::~Engine()
{
    renderer.waitForWorkFinished();
    resourceManager.cleanup();
    renderer.cleanup();
}

bool Engine::isRunning() const
{
    return _isRunning;
}

void Engine::update()
{
    _isRunning &= !glfwWindowShouldClose(Engine::get()->getMainWindow()->glfwWindow);
    if(!_isRunning)
        return;

    glfwPollEvents();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    inputManager.update();
    if(!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
    {
        mainCamera.update();
    }
    // Not sure about the order of UI & engine code

    ImGui::ShowDemoWindow();
    ImGui::Render();

    renderer.draw();
}