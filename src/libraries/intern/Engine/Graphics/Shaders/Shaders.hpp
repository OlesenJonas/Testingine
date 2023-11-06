#pragma once

#include "SPIRV-Reflect/spirv_reflect.h"
#include <Datastructures/Span.hpp>
#include <Datastructures/StringMap.hpp>
#include <fasttuple/fasttuple.hpp>
#include <span> //for better debug vis //TODO: natvis for custom span
#include <string_view>

namespace Material
{
    struct ParameterInfo;
}

namespace Shaders
{
    enum class Stage
    {
        Vertex,
        Fragment,
        Compute,
    };

    enum class Language
    {
        HLSL,
        GLSL,
    };

    struct StageCreateInfo
    {
        std::string_view sourcePath;
        Language sourceLanguage = Language::HLSL;
    };

    namespace Reflection
    {
        class Module
        {
          public:
            explicit Module(const Span<const uint32_t> spirv);
            ~Module();

            inline bool isInitialized() const { return parseResult == SPV_REFLECT_RESULT_SUCCESS; }

            const SpvReflectDescriptorBinding* findDescriptorBinding(const std::string& name) const;

            CTuple<StringMap<Material::ParameterInfo>, size_t> parseMaterialParams() const;
            CTuple<StringMap<Material::ParameterInfo>, size_t> parseMaterialInstanceParams() const;

          private:
            std::span<SpvReflectDescriptorBinding> getDescriptorBindings() const;

            SpvReflectResult parseResult = SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR;
            SpvReflectShaderModule spvModule;
        };

        CTuple<StringMap<Material::ParameterInfo>, size_t>
        parseBufferBinding(const SpvReflectDescriptorBinding& binding, bool isUniform = true);

        bool isBufferBinding(const SpvReflectDescriptorBinding& binding);
    } // namespace Reflection
} // namespace Shaders