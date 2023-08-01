#include "HLSL.hpp"

// TODO: create a minimal header that defines just the windows types needed for DXC
//       also: how to handle non-windows
#include <windows.h>
// why doesnt dxc include these?
#include <atlbase.h>

#include <dxc/dxcapi.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

std::vector<uint32_t> compileHLSL(std::string_view path, Shaders::Stage stage)
{
    std::wstring filename{path.begin(), path.end()};

    /*
        TODO: I dont really _understand_ how this works, this is just the sample code!
    */

    // https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/hlsl.adoc#runtime-compilation-using-the-library
    // https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll
    HRESULT hres;

    // todo: which of these can(/do I have to) create just once?
    CComPtr<IDxcLibrary> library;
    hres = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    if(FAILED(hres))
    {
        throw std::runtime_error("Could not init DXC Library");
    }

    // Initialize DXC compiler
    CComPtr<IDxcCompiler3> compiler;
    hres = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if(FAILED(hres))
    {
        throw std::runtime_error("Could not init DXC Compiler");
    }

    // Initialize DXC utility
    CComPtr<IDxcUtils> utils;
    hres = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    if(FAILED(hres))
    {
        throw std::runtime_error("Could not init DXC Utiliy");
    }

    CComPtr<IDxcVersionInfo2> info;
    hres = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&info));
    if(FAILED(hres))
    {
        throw std::runtime_error("Could not init DXC Utiliy");
    }
    uint32_t compilerMajor = 1;
    uint32_t compilerMinor = 0;
    info->GetVersion(&compilerMajor, &compilerMinor);
    UINT32 commitCount = 0;
    CComHeapPtr<char> commitHash;
    info->GetCommitInfo(&commitCount, &commitHash);

    // Create default include handler. (You can create your own...)
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    utils->CreateDefaultIncludeHandler(&pIncludeHandler);

    LPCWSTR targetProfile{};
    switch(stage)
    {
    case Shaders::Stage::Vertex:
        targetProfile = L"vs_6_6";
        break;
    case Shaders::Stage::Fragment:
        targetProfile = L"ps_6_6";
        break;
    case Shaders::Stage::Compute:
        targetProfile = L"cs_6_6";
        break;
    }

    // Configure the compiler arguments for compiling the HLSL shader to SPIR-V
    std::vector<LPCWSTR> arguments = {
        // (Optional) name of the shader file to be displayed e.g. in an error message
        filename.c_str(),
        // Shader main entry point
        L"-E",
        L"main",
        L"-HV 2021", // for templates etc
        // Shader target profile
        L"-T",
        targetProfile,
        // Compile to SPIRV
        L"-spirv",
        L"-fspv-target-env=vulkan1.3",
        L"-fvk-use-scalar-layout",
    // L"-fspv-reflect",
    // TODO: not sure if PR was merged into main yet, needed for RWStructuredBuffer arrays
    // L"fvk_allow_rwstructuredbuffer_arrays",

#ifndef NDEBUG
    // L"-fspv-debug=vulkan-with-source", //CURRENTLY BROKEN :/
#endif
    };

    // Load the HLSL text shader from disk
    CComPtr<IDxcBlobEncoding> pSource = nullptr;
    hres = utils->LoadFile(filename.c_str(), nullptr, &pSource);
    if(FAILED(hres))
    {
        throw std::runtime_error("Could not load shader file");
    }
    DxcBuffer source{};
    source.Encoding = DXC_CP_ACP;
    source.Ptr = pSource->GetBufferPointer();
    source.Size = pSource->GetBufferSize();
    const char* text = (char*)source.Ptr;

    CComPtr<IDxcResult> pResults;
    hres = compiler->Compile(
        &source, //
        arguments.data(),
        (uint32_t)arguments.size(),
        pIncludeHandler,
        IID_PPV_ARGS(&pResults));

    // Print errors if present.
    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    // Note that d3dcompiler would return null if no errors or warnings are present.
    // IDxcCompiler3::Compile will always return an error buffer, but its length
    // will be zero if there are no warnings or errors.
    if(pErrors != nullptr && pErrors->GetStringLength() != 0)
    {
        /*
            Note:
                If this fails because "DXC hasnt been compiled with spirv codegen"
                then an outdated version (windows sdk?) of DXC is used instead of the vulkan sdk.
                In my case that happended because the Vulkan SDK didnt include debug versions of DXC (?)
                and couldnt be linked against. Switching to vcpkgs's dxc fixed that
        */
        wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());
        std::cout << std::endl;
    }

    // Quit if the compilation failed.
    HRESULT hrStatus;
    pResults->GetStatus(&hrStatus);
    if(FAILED(hrStatus))
    {
        wprintf(L"Compilation Failed\n");
        return {};
    }

    // Get compilation result
    CComPtr<IDxcBlob> code;
    pResults->GetResult(&code);

    std::vector<uint32_t> shaderBinary;
    shaderBinary.resize(code->GetBufferSize() / sizeof(uint32_t));
    memcpy(shaderBinary.data(), code->GetBufferPointer(), code->GetBufferSize());
    return shaderBinary;
}