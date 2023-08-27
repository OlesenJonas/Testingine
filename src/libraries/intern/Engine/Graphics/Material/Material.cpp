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
        auto* ptr = (uint32_t*)(&paramBuffer->writeBuffer[parameterInfo.byteOffsetInBuffer]);
        *ptr = index;
        *ResourceManager::impl()->get<bool>(handle) = true;
    }
} // namespace Material

namespace MaterialInstance
{
    void setResource(Handle handle, std::string_view name, ResourceIndex index)
    {
        Material::Handle parent = *ResourceManager::impl()->get<Material::Handle>(handle);

        const auto& parameterLUT = ResourceManager::impl()->get<Material::InstanceParameterMap>(parent)->map;
        const auto& iterator = parameterLUT.find(name);
        if(iterator == parameterLUT.end())
        {
            // todo: emit some warning
            return;
        }
        const auto& parameterInfo = iterator->second;

        auto* paramBuffer = ResourceManager::impl()->get<MaterialInstance::ParameterBuffer>(handle);
        auto* ptr = (uint32_t*)(&paramBuffer->writeBuffer[parameterInfo.byteOffsetInBuffer]);
        *ptr = index;
        *ResourceManager::impl()->get<bool>(handle) = true;
    }
} // namespace MaterialInstance