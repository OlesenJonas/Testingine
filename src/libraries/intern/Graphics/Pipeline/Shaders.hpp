#pragma once

#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <string_view>
#include <vulkan/vulkan_core.h>

struct ShaderIncludeHandler : public shaderc::CompileOptions::IncluderInterface
{
    shaderc_include_result* GetInclude(
        const char* requested_source,
        shaderc_include_type type,
        const char* requesting_source,
        size_t include_depth) override;

    void ReleaseInclude(shaderc_include_result* data) override;
};

// todo: custom shader_kind wrapper here?
VkShaderModule compileGLSL(std::string_view pat, shaderc_shader_kind shaderType);