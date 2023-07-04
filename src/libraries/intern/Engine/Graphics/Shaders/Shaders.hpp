#pragma once

#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace Shaders
{
    enum class Stage
    {
        Vertex,
        Fragment,
        Compute
    };
}

struct ShaderIncludeHandler : public shaderc::CompileOptions::IncluderInterface
{
    shaderc_include_result* GetInclude(
        const char* requested_source,
        shaderc_include_type type,
        const char* requesting_source,
        size_t include_depth) override;

    void ReleaseInclude(shaderc_include_result* data) override;
};

std::vector<uint32_t> compileGLSL(std::string_view path, shaderc_shader_kind shaderType);