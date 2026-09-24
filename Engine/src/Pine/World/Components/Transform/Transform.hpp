#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{

    // Position, rotation and scale relative to the parent entity's transform.
    //
    // A child sits in its parent's space the way a mesh sits in its transformation matrix: scaled,
    // then rotated, then translated. Down the hierarchy rotations compose and scales multiply
    // component by component, so a non-uniform scale never skews a rotated child.
    class Transform final : public Component
    {
    private:
        Vector3f m_LocalPosition = Vector3f(0.f);
        Vector3f m_LocalScale = Vector3f(1.f);
        Quaternion m_LocalRotation = glm::identity<glm::quat>();

        // The world-space values, composed from the local ones and the parent's. They are
        // recomputed on the first read after this transform or one of its ancestors changes, which
        // is safe for const readers because the world is only touched from the main thread.
        mutable Vector3f m_Position = Vector3f(0.f);
        mutable Quaternion m_Rotation = glm::identity<glm::quat>();
        mutable Vector3f m_Scale = Vector3f(1.f);
        mutable Matrix4f m_TransformationMatrix = Matrix4f(1.f);

        mutable bool m_IsWorldStale = true;

        const Transform* GetParentTransform() const;
        void UpdateWorldTransform() const;

        struct TransformSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(LocalPosition, Pine::Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(LocalRotation, Pine::Serialization::DataType::Quaternion);
            PINE_SERIALIZE_PRIMITIVE(LocalScale, Pine::Serialization::DataType::Vec3);
        };
    public:
        explicit Transform();

        // Marks this transform and every transform below it as changed, so their world values are
        // recomputed on the next read. Every setter calls this, and so does Entity::SetParent.
        void SetDirty();

        void OnCreated() override;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;

        const Vector3f& GetLocalPosition() const;
        void SetLocalPosition(const Vector3f& position);

        const Quaternion& GetLocalRotation() const;
        void SetLocalRotation(const Quaternion& rotation);

        const Vector3f& GetLocalScale() const;
        void SetLocalScale(const Vector3f& scale);

        // World space. The setters store the local value that produces the requested world value.
        // Under a parent scaled to zero on some axis no such value exists, and the result is not finite.
        Vector3f GetPosition() const;
        void SetPosition(const Vector3f& position);

        Quaternion GetRotation() const;
        void SetRotation(const Quaternion& rotation);

        Vector3f GetScale() const;
        void SetScale(const Vector3f& scale);

        Vector3f GetForward() const;
        Vector3f GetRight() const;
        Vector3f GetUp() const;

        Vector3f GetEulerAngles() const;
        void SetEulerAngles(Vector3f angle);

        const Matrix4f& GetTransformationMatrix() const;
    };

}
