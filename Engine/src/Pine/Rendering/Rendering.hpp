#pragma once

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

}