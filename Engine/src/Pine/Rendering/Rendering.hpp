#pragma once
#include <cstdint>

namespace Pine::Rendering
{

    enum class CoordinateSystem
    {
        Screen,
        World
    };

    enum class TextureSamplers
    {
        Diffuse = 0,
        Specular = 1,
        EnvironmentMap = 2
    };

    inline constexpr std::uint32_t PixelsPerUnit = 64;

    namespace Internal
    {
        // Hands the renderer's shared GLSL constants to the ShaderSpecificationRegistry. Must run
        // before the engine assets load, since every shader compiles as it loads.
        void RegisterShaderSpecifications();
    }

}