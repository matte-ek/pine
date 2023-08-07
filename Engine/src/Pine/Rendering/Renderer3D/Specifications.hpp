#pragma once

namespace Pine::Renderer3D::Specifications
{
    namespace General
    {
        constexpr int DYNAMIC_LIGHT_COUNT = 10;
    }

    namespace Buffers
    {
        constexpr int VERTEX_ARRAY_BUFFER = 0;
        constexpr int NORMAL_ARRAY_BUFFER = 1;
        constexpr int UV_ARRAY_BUFFER = 2;
        constexpr int TANGENT_ARRAY_BUFFER = 3;
    }

    namespace ShaderStorages
    {
        constexpr int MATRICES = 0;
        constexpr int MATERIAL = 1;
        constexpr int LIGHTS = 2;
    }
}