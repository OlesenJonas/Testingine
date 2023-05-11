#pragma once

#include <Engine/Graphics/Renderer/VulkanRenderer.hpp>
#include <Engine/Misc/StringHash.hpp>

struct ParameterInfo;
struct MaterialInstance;
struct ResourceManager;

struct ParameterBuffer
{
    void setResource(std::string_view name, uint32_t index);
    void pushChanges();
    Handle<Buffer> getGPUBuffer();

  private:
    using ParameterMap = std::unordered_map<std::string, ParameterInfo, StringHash, std::equal_to<>>;

    ParameterMap* lut = nullptr;

    uint32_t bufferSize = 0;

    char* cpuBuffer = nullptr;

    uint8_t currentBufferIndex = 0;
    Handle<Buffer> gpuBuffers[VulkanRenderer::FRAMES_IN_FLIGHT] = {Handle<Buffer>::Invalid()};

    friend MaterialInstance;
    friend ResourceManager;
};