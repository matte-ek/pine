#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/Graphics/ShaderStorage/ShaderStorage.hpp"

#include "Specifications.hpp"

namespace Pine::Renderer3D::ShaderStorages
{
    struct MatrixData
    {
        Matrix4f Projection;
        Matrix4f View;
    };

    struct InstanceData
    {
        struct Instance
        {
            Matrix4f TransformationMatrix;
            int LightIndices[8];
        }Instances[Specifications::General::MAX_INSTANCE_COUNT];
    };

    struct MaterialProperties
    {
        Vector3f DiffuseColor;
        float Pad0;
        Vector3f SpecularColor;
        float Pad1;
        Vector3f AmbientColor;
        float Shininess;
        float UVScale;
        float Pad3;
        float Pad4;
        float Pad5;
    };

    struct MaterialData
    {
        MaterialProperties Properties[8];
    };

    struct LightsData
    {
        struct Light
        {
            Vector3f Position = Vector3f(0.f);
            float Pad0 = 0;

            // Points *towards* the light (the opposite of the light's forward). See the Light
            // struct in data/engine/shaders/3d/shared/common.glsl for the convention.
            Vector3f DirectionToLight = Vector3f(0.f);
            float Pad1 = 0;

            Vector3f Color = Vector3f(0.f);
            float Pad2 = 0;

            // World units at which this light's contribution reaches exactly zero. The falloff is
            // windowed against it (see CalculatePositionalLight), so it is a hard cutoff rather
            // than an asymptote - which is what lets a shadow far plane sit here.
            float Range = 0.f;
            float Pad2a = 0;
            float Pad2b = 0;

            // Cosines of the spotlight cone half-angles. AddLight guarantees Outer < Inner.
            float CutOffOuter = 0.f;

            float CutOffInner = 0.f;

            // Index of this light's first ShadowView, or -1 when it casts none. Deliberately -1 and
            // not 0: index 0 is already overloaded in this buffer (the directional light lives
            // there AND Instance::LightIndices uses 0 to mean "slot empty"), and a second sentinel
            // colliding with the first is a bug waiting to be written.
            int ShadowViewIndex = -1;

            // 1 for a spot, 6 for a point light's cube faces.
            int ShadowViewCount = 0;

            // Multiplier on the shadow term, faded 0..1 when a light gains or loses a shadow tile.
            // Lives here from the start on purpose: retrofitting a factor the shader must multiply
            // by, after the lookup already works, means finding every place that forgot to.
            float ShadowFade = 0.f;
        }Lights[Specifications::General::DYNAMIC_LIGHT_COUNT];
    };

    // One entry per live ShadowView. Mirrors the ShadowView struct in
    // data/engine/shaders/3d/shared/common.glsl - std140 puts every member on a 16-byte boundary
    // here, so the two layouts agree without explicit padding.
    struct ShadowViewData
    {
        struct View
        {
            Matrix4f ViewProjection = Matrix4f(1.f);

            // Where this view's tile sits in the atlas, in normalized atlas UV: xy = origin,
            // zw = size. The shader maps its own [0,1] projection into this rect.
            Vector4f TileRect = Vector4f(0.f);

            // x = world size of one of this view's texels per unit distance from the view origin
            // (zero for an orthographic view, whose texels do not change size with distance),
            // y = normal offset in texels, z = shadow strength (the fade applied when a light gains
            // or loses its tile), w = unused.
            Vector4f Params = Vector4f(0.f);
        }Views[Specifications::Shadows::SHADOW_VIEW_COUNT];
    };

    struct WorldData
    {
        Vector4f AmbientColor = Vector4f(0.f);
        Vector4f FogColor = Vector4f(0.f);
        Vector4f FogSettings = Vector4f(0.f);
    };

    inline Graphics::ShaderStorage<MatrixData> Matrix(Specifications::ShaderStorages::MATRICES, "Matrices");
    inline Graphics::ShaderStorage<InstanceData> Instance(Specifications::ShaderStorages::INSTANCE, "Instances");
    inline Graphics::ShaderStorage<MaterialData> Material(Specifications::ShaderStorages::MATERIAL, "Material");
    inline Graphics::ShaderStorage<LightsData> Lights(Specifications::ShaderStorages::LIGHTS, "Lights");
    inline Graphics::ShaderStorage<ShadowViewData> ShadowViews(Specifications::ShaderStorages::SHADOW_VIEWS, "ShadowViews");
    inline Graphics::ShaderStorage<WorldData> World(Specifications::ShaderStorages::WORLD, "World");
}