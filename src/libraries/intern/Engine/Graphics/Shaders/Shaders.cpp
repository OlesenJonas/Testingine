#include "Shaders.hpp"
#include <Engine/Graphics/Material/Material.hpp>

Shaders::Reflection::Module::Module(const Span<const uint32_t> spirv)
{
    parseResult = spvReflectCreateShaderModule(spirv.size() * 4, spirv.data(), &spvModule);

    if(!isInitialized())
        return;
}

Shaders::Reflection::Module::~Module()
{
    if(isInitialized())
        spvReflectDestroyShaderModule(&spvModule);
}

const SpvReflectDescriptorBinding*
Shaders::Reflection::Module::findDescriptorBinding(const std::string& name) const
{
    for(const auto& binding : getDescriptorBindings())
    {
        if(strcmp(binding.name, name.c_str()) == 0)
            return &binding;
    }
    return nullptr;
}

CTuple<StringMap<Material::ParameterInfo>, size_t> Shaders::Reflection::Module::parseMaterialParams() const
{
    const SpvReflectDescriptorBinding* binding = findDescriptorBinding("g_ConstantBuffer_MaterialParameters");

    CTuple<StringMap<Material::ParameterInfo>, size_t> ret{{}, 0};

    if(binding == nullptr || !isBufferBinding(*binding))
        return ret;

    return Shaders::Reflection::parseBufferBinding(*binding);
}
CTuple<StringMap<Material::ParameterInfo>, size_t> Shaders::Reflection::Module::parseMaterialInstanceParams() const
{
    const SpvReflectDescriptorBinding* binding =
        findDescriptorBinding("g_ConstantBuffer_MaterialInstanceParameters");

    CTuple<StringMap<Material::ParameterInfo>, size_t> ret{{}, 0};

    if(binding == nullptr || !isBufferBinding(*binding))
        return ret;

    return Shaders::Reflection::parseBufferBinding(*binding);
}

std::span<SpvReflectDescriptorBinding> Shaders::Reflection::Module::getDescriptorBindings() const
{
    return {spvModule.descriptor_bindings, spvModule.descriptor_binding_count};
}

CTuple<StringMap<Material::ParameterInfo>, size_t>
Shaders::Reflection::parseBufferBinding(const SpvReflectDescriptorBinding& binding, bool isUniform)
{
    // TODO: return error code if binding type is nont buffer

    CTuple<StringMap<Material::ParameterInfo>, size_t> ret;
    auto& [memberMap, bufferSize] = ret;

    const auto& blockToParse = isUniform ? binding.block : binding.block.members[0];

    bufferSize = blockToParse.padded_size; //.padded_size or .size ?

    std::span<SpvReflectBlockVariable> members{blockToParse.members, blockToParse.member_count};

    for(auto& member : members)
    {
        assert(member.padded_size >= member.size);

        auto insertion = memberMap.try_emplace(
            member.name,
            Material::ParameterInfo{
                .byteSize = (uint16_t)member.padded_size,
                // not sure what absolute offset is, but .offset seems to work
                .byteOffsetInBuffer = (uint16_t)member.offset,
            });
        assert(insertion.second); // assertion must have happened, otherwise something is srsly wrong
    }

    return ret;
}

bool Shaders::Reflection::isBufferBinding(const SpvReflectDescriptorBinding& binding)
{
    return binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
           binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
           binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
}