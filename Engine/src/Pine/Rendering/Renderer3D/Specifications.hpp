#pragma once

namespace Pine::Renderer3D::Specifications
{
    namespace General
    {
        constexpr int DYNAMIC_LIGHT_COUNT = 32;
        constexpr int MAX_INSTANCE_COUNT = 128;
    }

    namespace Samplers
    {
        constexpr int DIFFUSE = 0;
        constexpr int SPECULAR = 1;
        constexpr int NORMAL = 2;
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
        constexpr int TRANSFORM = 1;
        constexpr int MATERIAL = 2;
        constexpr int LIGHTS = 3;
    }
}