#include "ParameterBuffer.hpp"
#include "Material.hpp"

#include <Engine/ResourceManager/ResourceManager.hpp>
#include <type_traits>

void ParameterBuffer::setResource(std::string_view name, uint32_t index)
{
    // currently relying on this being pointers for direct access.
    // Otherwise would need rm->get(...)->LUT or smth
    static_assert(std::is_pointer_v<decltype(Material::parametersLUT)>);
    static_assert(std::is_same_v<decltype(Material::parametersLUT), decltype(lut)>);

    const auto& iterator = lut->find(name);
    if(iterator == lut->end())
    {
        // todo: emit some warning
        return;
    }
    const auto& parameterInfo = iterator->second;
    auto* ptr = (uint32_t*)(&(cpuBuffer[parameterInfo.byteOffsetInBuffer]));
    *ptr = index;
}

void ParameterBuffer::pushChanges()
{
    // Copy changes over to next vulkan buffer and set it as active
    uint8_t newIndex = (currentBufferIndex + 1) % VulkanDevice::FRAMES_IN_FLIGHT;
    Buffer* paramsBuffer = ResourceManager::get()->get(gpuBuffers[newIndex]);
    memcpy(paramsBuffer->gpuBuffer.ptr, cpuBuffer, bufferSize);
    currentBufferIndex = newIndex;
}

Handle<Buffer> ParameterBuffer::getGPUBuffer()
{
    return gpuBuffers[currentBufferIndex];
}
