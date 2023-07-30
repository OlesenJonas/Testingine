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

    // Create default Samplers

// WARNING when changing these, also change them in Shaders/Bindless/Sampler.hlsl
#define DEFAULT_SAMPLER_LINEAR_REPEAT 0
#define DEFAULT_SAMPLER_LINEAR_CLAMP 1
#define DEFAULT_SAMPLER_NEAREST 2
    constexpr Sampler::Info defaultInfos[] = {
        {
            .magFilter = Sampler::Filter::Linear,
            .minFilter = Sampler::Filter::Linear,
            .mipMapFilter = Sampler::Filter::Linear,
            .addressModeU = Sampler::AddressMode::Repeat,
            .addressModeV = Sampler::AddressMode::Repeat,
            .addressModeW = Sampler::AddressMode::Repeat,
        },
        {
            .magFilter = Sampler::Filter::Linear,
            .minFilter = Sampler::Filter::Linear,
            .mipMapFilter = Sampler::Filter::Linear,
            .addressModeU = Sampler::AddressMode::ClampEdge,
            .addressModeV = Sampler::AddressMode::ClampEdge,
            .addressModeW = Sampler::AddressMode::ClampEdge,
        },
        {
            .magFilter = Sampler::Filter::Nearest,
            .minFilter = Sampler::Filter::Nearest,
            .mipMapFilter = Sampler::Filter::Nearest,
            .addressModeU = Sampler::AddressMode::Repeat,
            .addressModeV = Sampler::AddressMode::Repeat,
            .addressModeW = Sampler::AddressMode::Repeat,
        },
    };
    {
        auto linearSampler =
            resourceManager.createSampler(Sampler::Info{defaultInfos[DEFAULT_SAMPLER_LINEAR_REPEAT]});
        assert(resourceManager.get(linearSampler)->info == defaultInfos[DEFAULT_SAMPLER_LINEAR_REPEAT]);
        assert(resourceManager.get(linearSampler)->resourceIndex == DEFAULT_SAMPLER_LINEAR_REPEAT);
    }
    {
        auto linearSampler =
            resourceManager.createSampler(Sampler::Info{defaultInfos[DEFAULT_SAMPLER_LINEAR_CLAMP]});
        assert(resourceManager.get(linearSampler)->info == defaultInfos[DEFAULT_SAMPLER_LINEAR_CLAMP]);
        assert(resourceManager.get(linearSampler)->resourceIndex == DEFAULT_SAMPLER_LINEAR_CLAMP);
    }
    {
        auto nearestSampler = resourceManager.createSampler(Sampler::Info{defaultInfos[DEFAULT_SAMPLER_NEAREST]});
        assert(resourceManager.get(nearestSampler)->info == defaultInfos[DEFAULT_SAMPLER_NEAREST]);
        assert(resourceManager.get(nearestSampler)->resourceIndex == DEFAULT_SAMPLER_NEAREST);
    }

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