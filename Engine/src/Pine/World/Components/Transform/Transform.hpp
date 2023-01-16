#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"

namespace Pine
{

    class Transform : public IComponent
    {
    private:
        Matrix4f m_TransformationMatrix = Matrix4f(1.f);

        void CalculateTransformationMatrix();
    public:
        explicit Transform();

        void OnRender(float deltaTime) override;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;

        Vector3f GetPosition() const;
        Quaternion GetRotation() const;
        Vector3f GetScale() const;

        Vector3f GetForward() const;
        Vector3f GetRight() const;
        Vector3f GetUp() const;

        const Matrix4f& GetTransformationMatrix() const;

        Vector3f GetEulerAngles() const;
        void SetEulerAngles(Vector3f angle);

        Vector3f LocalPosition = Vector3f(0.f);
        Vector3f LocalScale = Vector3f(1.f);
        Quaternion LocalRotation = glm::identity<glm::quat>();
    };

}