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
    Material::Handle writeToSwapchainMat;

    void createDefaultAssets();
    void createDefaultSamplers();
    void createDefaultMeshes();
    void createDefaultMaterialAndInstances();
    void createDefaultComputeShaders();
    void createRendertargets();
    void createDefaultTextures();

    Handle<ComputeShader> debugMipFillShader;
    Handle<ComputeShader> prefilterEnvShader;
    Handle<ComputeShader> integrateBrdfShader;

    Texture::Handle mipDebugTex;
    Texture::Handle defaultHDRI;
    Texture::Handle brdfIntegralTex;

    MaterialInstance::Handle unlitMatInst;
    MaterialInstance::Handle equiSkyboxMatInst;
    MaterialInstance::Handle cubeSkyboxMatInst;

    Mesh::Handle triangleMesh;

    Handle<ComputeShader> equiToCubeShader;
    Handle<ComputeShader> irradianceCalcShader;
    struct SkyboxTextures
    {
        Texture::Handle cubeMap;
        Texture::Handle irradianceMap;
        Texture::Handle prefilteredMap;
    };
    SkyboxTextures
    generateSkyboxTextures(uint32_t hdriCubeRes, uint32_t irradianceRes, uint32_t prefilteredEnvMapBaseSize);

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

    // TODO: keep shader and c++ versions of structs synced
    struct GPUMeshData
    {
        // uint32_t indexCount;
        uint32_t additionalUVCount; // TODO: store full attribute stride directly?
        uint32_t meshletCount;
        // ResourceIndex indexBuffer;
        ResourceIndex positionBuffer;
        ResourceIndex attributeBuffer;
        ResourceIndex meshletVertexIndices;
        ResourceIndex meshletPrimitiveIndices;
        ResourceIndex meshletDescriptors;
    };
    struct GPUMeshDataBuffer
    {
        const int limit = 10000;
        Buffer::Handle buffer;
        // TODO: bitset and/or full pool logic instead
        uint32_t freeIndex = 0;
    } gpuMeshDataBuffer;
    struct InstanceInfo
    {
        glm::mat4 transform;
        glm::mat4 invTranspTransform;
        uint32_t meshDataIndex;
        uint32_t materialIndex;
        ResourceIndex materialParamsBuffer;
        ResourceIndex materialInstanceParamsBuffer;
    };
    struct InstanceInfoBuffer
    {
        const int limit = 10000;
        Buffer::Handle buffer;
        // TODO: bitset and/or full pool logic instead
        uint32_t freeIndex = 0;
    } gpuInstanceInfoBuffer;

    // TODO: from shader file
    struct GraphicsPushConstants
    {
        // Resolution, matrices (differs in eg. shadow and default pass)
        ResourceIndex renderInfoBuffer;

        ResourceIndex instanceBuffer;

        uint32_t indexInInstanceBuffer;
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