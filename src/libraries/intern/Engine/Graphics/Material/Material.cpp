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

    template <typename T>
    void setValue(Handle handle, std::string_view name, T value)
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
        auto* ptr = (T*)(&paramBuffer->cpuBuffer[parameterInfo.byteOffsetInBuffer]);
        *ptr = value;
        *ResourceManager::impl()->get<bool>(handle) = true;
    }

    template void setValue<uint32_t>(Handle, std::string_view, uint32_t);
    template void setValue<float>(Handle, std::string_view, float);
    template void setValue<glm::vec2>(Handle, std::string_view, glm::vec2);
    template void setValue<glm::vec4>(Handle, std::string_view, glm::vec4);

} // namespace MaterialInstance