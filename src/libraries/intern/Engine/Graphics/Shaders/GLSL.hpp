#pragma once

#include "Shaders.hpp"

#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <string_view>

struct ShaderIncludeHandler : public shaderc::CompileOptions::IncluderInterface
{
    shaderc_include_result* GetInclude(
        const char* requested_source,
        shaderc_include_type type,
        const char* requesting_source,
        size_t include_depth) override;

    void ReleaseInclude(shaderc_include_result* data) override;
};

std::vector<uint32_t> compileGLSL(std::string_view path, Shaders::Stage stage);