#pragma once

#include "Pine/Graphics/Interfaces/IUniformVariable.hpp"

namespace Pine::Graphics
{

    class GLUniformVariable : public IUniformVariable
    {
    private:
        std::int32_t m_Id = 0;
    public:
        explicit GLUniformVariable(std::int32_t id);

        void LoadInteger(int value) override;
        void LoadFloat(float value) override;

        void LoadVector2(const Vector2f& value) override;
        void LoadVector3(const Vector3f& value) override;
        void LoadVector4(const Vector4f& value) override;

        void LoadVector2(const Vector2i& value) override;
        void LoadVector3(const Vector3i& value) override;
        void LoadVector4(const Vector4i& value) override;

        void LoadMatrix4(const Matrix4f& value) override;
    };

}