#include "Material.hpp"
#include <Engine/ResourceManager/ResourceManager.hpp>

namespace Material
{
    void setResource(Handle handle, std::string_view name, ResourceIndex index)
    {
        const auto& parameterLUT = ResourceManager::impl()->get<Material::ParameterMap>(handle)->map;
        const auto& iterator = parameterLUT.find(name);
        if(iterator == parameterLUT.end())
        {
            // todo: emit some warning
            return;
        }
        const auto& parameterInfo = iterator->second;

        auto* paramBuffer = ResourceManager::impl()->get<Material::ParameterBuffer>(handle);
        auto* ptr = (uint32_t*)(&paramBuffer->cpuBuffer[parameterInfo.byteOffsetInBuffer]);
        *ptr = index;
        *ResourceManager::impl()->get<bool>(handle) = true;
    }
} // namespace Material

namespace MaterialInstance
{
    // TODO: refactor to share code
    void setResource(Handle handle, std::string_view name, ResourceIndex index)
    {
        Material::Handle parent = *ResourceManager::impl()->get<Material::Handle>(handle);

        const auto& parameterLUT = ResourceManager::impl()->get<Material::InstanceParameterMap>(parent)->map;
        const auto& iterator = parameterLUT.find(name);
        if(iterator == parameterLUT.end())
        {
            // todo: emit some warning
            BREAKPOINT;
            return;
        }
        const auto& parameterInfo = iterator->second;

        auto* paramBuffer = ResourceManager::impl()->get<MaterialInstance::ParameterBuffer>(handle);
        auto* ptr = (uint32_t*)(&paramBuffer->cpuBuffer[parameterInfo.byteOffsetInBuffer]);
        *ptr = index;
        *ResourceManager::impl()->get<bool>(handle) = true;
    }

    void setFloat(Handle handle, std::string_view name, float value)
    {
        Material::Handle parent = *ResourceManager::impl()->get<Material::Handle>(handle);

        const auto& parameterLUT = ResourceManager::impl()->get<Material::InstanceParameterMap>(parent)->map;
        const auto& iterator = parameterLUT.find(name);
        if(iterator == parameterLUT.end())
        {
            // todo: emit some warning
            BREAKPOINT;
            return;
        }
        const auto& parameterInfo = iterator->second;

        auto* paramBuffer = ResourceManager::impl()->get<MaterialInstance::ParameterBuffer>(handle);
        auto* ptr = (float*)(&paramBuffer->cpuBuffer[parameterInfo.byteOffsetInBuffer]);
        *ptr = value;
        *ResourceManager::impl()->get<bool>(handle) = true;
    }

    void setFloat4(Handle handle, std::string_view name, glm::vec4 value)
    {
        Material::Handle parent = *ResourceManager::impl()->get<Material::Handle>(handle);

        const auto& parameterLUT = ResourceManager::impl()->get<Material::InstanceParameterMap>(parent)->map;
        const auto& iterator = parameterLUT.find(name);
        if(iterator == parameterLUT.end())
        {
            // todo: emit some warning
            BREAKPOINT;
            return;
        }
        const auto& parameterInfo = iterator->second;

        auto* paramBuffer = ResourceManager::impl()->get<MaterialInstance::ParameterBuffer>(handle);
        auto* ptr = (glm::vec4*)(&paramBuffer->cpuBuffer[parameterInfo.byteOffsetInBuffer]);
        *ptr = value;
        *ResourceManager::impl()->get<bool>(handle) = true;
    }

    void setUint(Handle handle, std::string_view name, uint32_t value)
    {
        Material::Handle parent = *ResourceManager::impl()->get<Material::Handle>(handle);

        const auto& parameterLUT = ResourceManager::impl()->get<Material::InstanceParameterMap>(parent)->map;
        const auto& iterator = parameterLUT.find(name);
        if(iterator == parameterLUT.end())
        {
            // todo: emit some warning
            BREAKPOINT;
            return;
        }
        const auto& parameterInfo = iterator->second;

        auto* paramBuffer = ResourceManager::impl()->get<MaterialInstance::ParameterBuffer>(handle);
        auto* ptr = (uint32_t*)(&paramBuffer->cpuBuffer[parameterInfo.byteOffsetInBuffer]);
        *ptr = value;
        *ResourceManager::impl()->get<bool>(handle) = true;
    }
} // namespace MaterialInstance