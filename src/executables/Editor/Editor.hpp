#pragma once

#include "Scene/Scene.hpp"
#include <Datastructures/ThreadPool.hpp>
#include <ECS/ECS.hpp>
#include <Engine/Application/Application.hpp>
#include <Engine/Graphics/Buffer/Buffer.hpp>
#include <Engine/Graphics/Device/VulkanDevice.hpp>

class Editor final : public Application
{
  public:
    Editor();
    ~Editor() final;

    void run();

  private:
    void update();

    VkCommandBuffer updateDirtyMaterialParameters();

    uint32_t frameNumber = 0xFFFFFFFF;

    InputManager inputManager;

    ThreadPool threadPool;

    ECS ecs;
    Scene scene;

    // TODO: not sure if camera should be part of just Editor, or Application is general
    Camera mainCamera;

    Texture::Format depthFormat = Texture::Format::D32_FLOAT;
    Texture::Handle depthTexture;
    Texture::Format offscreenRTFormat = Texture::Format::R16_G16_B16_A16_FLOAT;
    Texture::Handle offscreenTexture;
    Mesh::Handle fullscreenTri;
    MaterialInstance::Handle writeToSwapchainMatInst;

    void createDefaultSamplers();

    void createDefaultTextures(VkCommandBuffer cmd);
    Texture::Handle mipDebugTex;
    Texture::Handle defaultHDRI;
    Texture::Handle brdfIntegralTex;

    Handle<ComputeShader> equiToCubeShader;
    Handle<ComputeShader> irradianceCalcShader;
    struct SkyboxTextures
    {
        Texture::Handle cubeMap;
        Texture::Handle irradianceMap;
        Texture::Handle prefilteredMap;
    };
    SkyboxTextures generateSkyboxTextures(
        VkCommandBuffer cmd,
        Texture::Handle equiSource,
        uint32_t hdriCubeRes,
        uint32_t irradianceRes,
        uint32_t prefilteredEnvMapBaseSize);

    VkCommandBuffer drawScene(int threadIndex);
    VkCommandBuffer drawUI(int threadIndex);

    // TODO: store somewhere else and keep synced with shader code version of struct
    struct RenderPassData
    {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 projView;
        glm::vec3 cameraPositionWS;
        float pad;
    };

    const int MAX_GPU_OBJECTS = 10000;
    Buffer::Handle transformsBuffer;
    // TODO: bitset and/or full pool logic instead
    uint32_t freeTransformIndex = 0;

    struct GraphicsPushConstants
    {
        // Resolution, matrices (differs in eg. shadow and default pass)
        ResourceIndex RenderInfoBuffer;
        // Buffer with material/-instance parameters
        ResourceIndex materialParamsBuffer;
        ResourceIndex materialInstanceParamsBuffer;
    };

    struct PerFrameData
    {
        Buffer::Handle renderPassDataBuffer;
    };

    PerFrameData perFrameData[VulkanDevice::FRAMES_IN_FLIGHT];
    inline PerFrameData& getCurrentFrameData()
    {
        return perFrameData[frameNumber % VulkanDevice::FRAMES_IN_FLIGHT];
    }

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void resizeCallback(GLFWwindow* window, int width, int height);
};