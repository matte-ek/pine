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

    struct MaterialData
    {
    };

    struct LightsData
    {
        struct Light
        {
        }Lights[Specifications::General::DYNAMIC_LIGHT_COUNT];
    };

    inline Graphics::ShaderStorage<MatrixData> Matrix(Specifications::ShaderStorages::MATRICES, "Matrix");
    inline Graphics::ShaderStorage<MatrixData> Material(Specifications::ShaderStorages::MATERIAL, "Material");
    inline Graphics::ShaderStorage<MatrixData> Lights(Specifications::ShaderStorages::LIGHTS, "Lights");
}