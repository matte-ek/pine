#pragma once

#include "Pine/Core/Math/Math.hpp"

namespace Pine::Graphics
{

    class IUniformVariable
    {
    public:
        virtual ~IUniformVariable() = default;

        virtual void LoadInteger(int value) = 0;
        virtual void LoadFloat(float value) = 0;

        virtual void LoadVector2(const Vector2f& value) = 0;
        virtual void LoadVector3(const Vector3f& value) = 0;
        virtual void LoadVector4(const Vector4f& value) = 0;

        virtual void LoadVector2(const Vector2i& value) = 0;
        virtual void LoadVector3(const Vector3i& value) = 0;
        virtual void LoadVector4(const Vector4i& value) = 0;

        virtual void LoadMatrix4(const Matrix4f& value) = 0;

    };

}