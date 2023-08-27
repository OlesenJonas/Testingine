#pragma once

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
    ECS::Entity sceneRoot;

    // TODO: not sure if camera should be part of just Editor, or Application is general
    Camera mainCamera;

    Texture::Format depthFormat = Texture::Format::d32_float;
    Texture::Handle depthTexture;

    // TODO: store somewhere else and keep synced with shader code version of struct
    struct RenderPassData
    {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 projView;
        glm::vec3 cameraPositionWS;
        float pad;
    };
    struct GPUObjectData
    {
        glm::mat4 modelMatrix;
    };
    struct BindlessIndices
    {
        // Frame globals
        uint32_t FrameDataBuffer;
        // Resolution, matrices (differs in eg. shadow and default pass)
        uint32_t RenderInfoBuffer;
        // Buffer with object transforms and index into that buffer
        uint32_t transformBuffer;
        uint32_t transformIndex;
        // Buffer with material/-instance parameters
        uint32_t materialParamsBuffer;
        uint32_t materialInstanceParamsBuffer;
    };

    struct PerFrameData
    {
        Buffer::Handle renderPassDataBuffer;
        // TODO: dont need to upload this every frame, most objects are static!!
        Buffer::Handle objectBuffer;
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