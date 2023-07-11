#pragma once

#include <string_view>

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
} // namespace Shaders