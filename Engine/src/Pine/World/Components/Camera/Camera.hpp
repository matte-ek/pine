#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"

namespace Pine
{

    class Camera : public IComponent
    {
    private:
        Matrix4f m_ProjectionMatrix = Matrix4f(1.f);
        Matrix4f m_ViewMatrix = Matrix4f(1.f);

        void BuildProjectionMatrix();
        void BuildViewMatrix();
    public:
        explicit Camera();

        void OnRender(float) override;

        const Matrix4f& GetProjectionMatrix() const;
        const Matrix4f& GetViewMatrix() const;
    };

}