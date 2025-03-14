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

    struct TransformData
    {
        Matrix4f TransformationMatrix[Specifications::General::MAX_INSTANCE_COUNT];
    };

    struct MaterialData
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

    struct LightsData
    {
        struct Light
        {
            Vector3f Position = Vector3f(0.f);
            float Pad0 = 0;

            Vector3f Rotation = Vector3f(0.f);
            float Pad1 = 0;

            Vector3f Color = Vector3f(0.f);
            float Pad2 = 0;

            Vector3f Attenuation = Vector3f(0.f);
            float Angle = 0.f;

            float AngleSmoothness = 0.f;
            float Pad4 = 0;
            float Pad5 = 0;
            float Pad6 = 0;
        }Lights[Specifications::General::DYNAMIC_LIGHT_COUNT];
    };

    inline Graphics::ShaderStorage<MatrixData> Matrix(Specifications::ShaderStorages::MATRICES, "Matrices");
    inline Graphics::ShaderStorage<TransformData> Transform(Specifications::ShaderStorages::TRANSFORM, "Transform");
    inline Graphics::ShaderStorage<MaterialData> Material(Specifications::ShaderStorages::MATERIAL, "Material");
    inline Graphics::ShaderStorage<LightsData> Lights(Specifications::ShaderStorages::LIGHTS, "Lights");
}