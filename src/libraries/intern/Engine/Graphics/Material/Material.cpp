#include "Material.hpp"
#include <Engine/ResourceManager/ResourceManager.hpp>

void Material::setResource(std::string_view name, uint32_t index)
{
    const auto& iterator = parametersLUT.find(name);
    if(iterator == parametersLUT.end())
    {
        // todo: emit some warning
        return;
    }
    const auto& parameterInfo = iterator->second;
    Buffer* writeBuffer = ResourceManager::get()->get(parameters.writeBuffer);
    auto* bufferPtr = static_cast<uint8_t*>(writeBuffer->gpuBuffer.ptr);
    auto* ptr = (uint32_t*)(&bufferPtr[parameterInfo.byteOffsetInBuffer]);
    *ptr = index;
}

void MaterialInstance::setResource(std::string_view name, uint32_t index)
{
    Material* parent = ResourceManager::get()->get(parentMaterial);

    const auto& iterator = parent->instanceParametersLUT.find(name);
    if(iterator == parent->instanceParametersLUT.end())
    {
        // todo: emit some warning
        return;
    }
    const auto& parameterInfo = iterator->second;
    Buffer* writeBuffer = ResourceManager::get()->get(parameters.writeBuffer);
    auto* bufferPtr = static_cast<uint8_t*>(writeBuffer->gpuBuffer.ptr);
    auto* ptr = (uint32_t*)(&bufferPtr[parameterInfo.byteOffsetInBuffer]);
    *ptr = index;
}

// void MaterialInstance::setResource(std::string_view name, uint32_t index) {}