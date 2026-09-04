#pragma once

namespace Pine::Renderer3D::Specifications
{
    namespace General
    {
        constexpr int DYNAMIC_LIGHT_COUNT = 32;
        constexpr int MAX_INSTANCE_COUNT = 512;

        // TODO: Stop with this.
        constexpr int INTERNAL_WIDTH = 1920;
        constexpr int INTERNAL_HEIGHT = 1080;
    }

    // How the light slots of a single object are laid out. Each slot holds an index into the light
    // buffer, so this has to match the lightIndices[] handling of the generic shader.
    //
    // COUNT is capped at 7 by the shader, not by anything here: the generic shader's varying block
    // carries lightDir[8], of which [0] is the directional light and [1..7] are these slots. Raising
    // COUNT past 7 means growing that array and both of its hand-unrolled write-outs, and costs
    // three interpolated floats per fragment on every material - so it is not free the way going
    // from 6 to 7 was.
    namespace ObjectLightSlots
    {
        constexpr int POINT_LIGHT_OFFSET = 0;
        constexpr int POINT_LIGHT_COUNT = 5;

        constexpr int SPOT_LIGHT_OFFSET = POINT_LIGHT_OFFSET + POINT_LIGHT_COUNT;

        // Two, so a hand-held light and a world light can shine on the same surface. At one, a
        // flashlight occupies the only slot every object it touches has, and no lamp, doorway or
        // window spot can light anything the player is pointing at.
        constexpr int SPOT_LIGHT_COUNT = 2;

        constexpr int COUNT = POINT_LIGHT_COUNT + SPOT_LIGHT_COUNT;
    }

    namespace Shadows
    {
        // Note: Changing this will require manual configuration,
        // configure ranges Shadows.cpp and the rendering shader.
        constexpr int CASCADE_COUNT = 2;

        // Squared, so the real distance is its square root - about 38.7 world units, not 1500. The
        // name has always lied. It now has exactly one reader: the last cascade's far plane, which
        // takes its sqrtf. The whole-scene distance test that gave the constant its squared form is
        // gone, replaced by culling each cascade against its own box.
        constexpr float MAX_SHADOW_DISTANCE = 1500.f;

        // How many ShadowViews can be live at once, across every shadow-casting light. A spot
        // spends one, a point light six. Injected into the shaders as a #define, so this is the
        // single place the count is written.
        constexpr int SHADOW_VIEW_COUNT = 32;
    }

    namespace PostProcessing
    {
        constexpr int AMBIENT_OCCLUSION_RES = 2;
    }

    namespace Samplers
    {
        constexpr int COUNT = 4;

        constexpr int BASE_DIFFUSE = 0;
        constexpr int BASE_SPECULAR = COUNT;
        constexpr int BASE_NORMAL = COUNT * 2;

        // One sampler for every shadow in the engine. There used to be a second at 16 for the
        // directional cascades' own texture array; folding them into the atlas deleted it.
        constexpr int SHADOW_ATLAS = 17;
    }

    namespace Buffers
    {
        constexpr int VERTEX_ARRAY_BUFFER = 0;
        constexpr int NORMAL_ARRAY_BUFFER = 1;
        constexpr int UV_ARRAY_BUFFER = 2;
        constexpr int TANGENT_ARRAY_BUFFER = 3;
    }

    namespace ShaderVersions
    {
        enum class Generic
        {
            Default = 0,
            Discard = (1 << 0),
            PerformanceFast = (1 << 1)
        };
    }

    namespace ShaderStorages
    {
        constexpr int MATRICES = 0;
        constexpr int INSTANCE = 1;
        constexpr int MATERIAL = 2;
        constexpr int LIGHTS = 3;
        // 4 was the Shadows block, holding the cascades' light-space matrices. Cascades are
        // ShadowViews now, so it is free.
        constexpr int WORLD = 5;
        constexpr int AO_DATA = 6;
        constexpr int SHADOW_VIEWS = 7;
    }
}