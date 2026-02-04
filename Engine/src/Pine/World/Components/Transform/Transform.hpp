#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{

    class Transform final : public Component
    {
    private:
        Matrix4f m_TransformationMatrix = Matrix4f(1.f);

        Vector3f m_LocalPosition = Vector3f(0.f);
        Vector3f m_LocalScale = Vector3f(1.f);
        Quaternion m_LocalRotation = glm::identity<glm::quat>();

        bool m_IsDirty = true;

        void CalculateTransformationMatrix();
    public:
        explicit Transform();

        // If the parent is static, and we want the transform to update, you have
        // to manually call SetDirty() to update the transformation matrix.
        void SetDirty();
        bool IsDirty() const;

        void OnRender(float deltaTime) override;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;

        const Vector3f& GetLocalPosition() const;
        void SetLocalPosition(const Vector3f& position);

        const Quaternion& GetLocalRotation() const;
        void SetLocalRotation(const Quaternion& rotation);

        const Vector3f& GetLocalScale() const;
        void SetLocalScale(const Vector3f& scale);

        Vector3f GetPosition() const;
        Quaternion GetRotation() const;
        Vector3f GetScale() const;

        Vector3f GetForward() const;
        Vector3f GetRight() const;
        Vector3f GetUp() const;

        Vector3f GetEulerAngles() const;
        void SetEulerAngles(Vector3f angle);

        const Matrix4f& GetTransformationMatrix() const;
    };

}