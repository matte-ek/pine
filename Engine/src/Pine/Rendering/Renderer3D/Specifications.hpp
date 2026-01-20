#pragma once

namespace Pine::Renderer3D::Specifications
{
    namespace General
    {
        constexpr int DYNAMIC_LIGHT_COUNT = 32;
        constexpr int DYNAMIC_LIGHT_OBJECT_COUNT = 6;
        constexpr int MAX_INSTANCE_COUNT = 512;

        // TODO: Stop with this.
        constexpr int INTERNAL_WIDTH = 1920;
        constexpr int INTERNAL_HEIGHT = 1080;
    }

    namespace Shadows
    {
        constexpr int SHADOW_MAP_RESOLUTION = 2048;

        // Note: Changing this will require manual configuration,
        // configure ranges Shadows.cpp and the rendering shader.
        constexpr int CASCADE_COUNT = 4;
    }

    namespace PostProcessing
    {
        constexpr int AMBIENT_OCCLUSION_RES = 2;
    }

    namespace Samplers
    {
        constexpr int DIFFUSE = 0;
        constexpr int SPECULAR = 1;
        constexpr int NORMAL = 2;
        constexpr int DIRECTIONAL_SHADOW_MAP = 3;
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
        constexpr int INSTANCE = 1;
        constexpr int MATERIAL = 2;
        constexpr int LIGHTS = 3;
        constexpr int SHADOWS = 4;
    }
}