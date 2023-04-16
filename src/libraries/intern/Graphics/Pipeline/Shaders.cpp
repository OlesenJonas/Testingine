#include "Shaders.hpp"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <intern/Engine/Engine.hpp>
#include <iostream>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <shaderc/status.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

shaderc_include_result* ShaderIncludeHandler::GetInclude(
    const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth)
{
    assert(type == shaderc_include_type_relative && "Absolute includes not yet supported");

    std::filesystem::path includePath{requesting_source};
    includePath = (includePath.parent_path() / requested_source).lexically_normal();

    std::string includePathAsString = includePath.generic_string();

    auto* result = new shaderc_include_result;

    char* nameBuffer = new char[includePathAsString.size() + 1];
    strncpy(nameBuffer, includePathAsString.c_str(), includePathAsString.size());
    result->source_name_length = includePathAsString.size();
    nameBuffer[result->source_name_length] = '\0';
    result->source_name = nameBuffer;

    // todo: read(Binary)File functions
    std::ifstream file(includePathAsString.c_str(), std::ios::ate | std::ios::binary);
    if(!file.is_open())
    {
        // todo: return a correct "error" include (look at shaderc documentation)
        //       that way shader compilation error returns correct name etc etc
        assert(false);
    }
    auto fileSize = (size_t)file.tellg();
    file.seekg(0);

    char* contentBuffer = new char[fileSize + 1];
    file.read(contentBuffer, fileSize);
    file.close();
    contentBuffer[fileSize] = '\0';
    result->content = contentBuffer;
    result->content_length = fileSize;

    return result;
};

void ShaderIncludeHandler::ReleaseInclude(shaderc_include_result* data)
{
    delete[] data->content;
    delete[] data->source_name;
};

std::vector<uint32_t> compileGLSL(std::string_view path, shaderc_shader_kind shaderType)
{
    std::string initialPath{path};

    // todo: read(Binary)File functions
    std::ifstream file(initialPath.c_str(), std::ios::ate | std::ios::binary);

    if(!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    auto fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    // todo: create just a single static compiler (and a global compiler option template) and keep that around?
    shaderc::Compiler compiler;
    assert(compiler.IsValid());
    shaderc::CompileOptions options;
    // options.AddMacroDefinition("Test","42");
    // options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetIncluder(std::make_unique<ShaderIncludeHandler>());
    // options.SetWarningsAsErrors();

    auto preprocessedResult =
        compiler.PreprocessGlsl(buffer.data(), buffer.size(), shaderType, initialPath.c_str(), options);
    if(preprocessedResult.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        std::cout << preprocessedResult.GetErrorMessage() << std::endl;
        assert(false);
    }
    // std::string preprocessedShader = std::string{preprocessedResult.cbegin(), preprocessedResult.cend()};
    // std::cout << preprocessedShader << std::endl;

    // todo: can use iterators as pointers for compilation directly, so that no temp string is needed?
    //       read docs if thats specified to work
    std::string preprocessedString{preprocessedResult.cbegin(), preprocessedResult.cend()};
    auto compilationResult = compiler.CompileGlslToSpv(
        preprocessedString.c_str(), preprocessedString.size(), shaderType, initialPath.c_str(), "main", options);
    if(compilationResult.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        std::cout << compilationResult.GetErrorMessage() << std::endl;
        assert(false);
    }

    return {compilationResult.cbegin(), compilationResult.cend()};
}