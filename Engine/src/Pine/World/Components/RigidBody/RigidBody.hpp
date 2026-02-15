#pragma once

#include <array>

#include "Pine/World/Components/Component/Component.hpp"

#include "physx/PxPhysicsAPI.h"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

namespace Pine
{
    enum class RigidBodyType
    {
        Static,
        Kinematic,
        Dynamic
    };

    class Collider;

    class RigidBody final : public Component
    {
    private:
        RigidBodyType m_RigidBodyType = RigidBodyType::Dynamic;

        float m_Mass = 1.0f;

        bool m_GravityEnabled = true;

        physx::PxRigidDynamic *m_RigidBody = nullptr;
        physx::PxTransform m_RigidBodyTransform;

        std::array<bool, 3> m_PositionLock = {false, false, false};
        std::array<bool, 3> m_RotationLock = {false, false, false};

        Collider *m_EngineCollider = nullptr;

        float m_MaxAngularVelocity = 0.0f;
        float m_MaxLinearVelocity = 0.0f;

        void UpdateColliders();
        void UpdateBody();

        struct RigidBodySerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Type, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(Mass, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(GravityEnabled, Serialization::DataType::Boolean);

            PINE_SERIALIZE_ARRAY_FIXED(PositionLock, bool);
            PINE_SERIALIZE_ARRAY_FIXED(RotationLock, bool);

            PINE_SERIALIZE_PRIMITIVE(MaxAngularVelocity, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(MaxLinearVelocity, Serialization::DataType::Float32);
        };
    public:
        RigidBody();

        physx::PxRigidDynamic *GetRigidBody() const;

        void ApplyForce(const Vector3f& force, physx::PxForceMode::Enum mode = physx::PxForceMode::Enum::eFORCE) const;

        void SetRigidBodyType(RigidBodyType type);
        RigidBodyType GetRigidBodyType() const;

        void SetMass(float mass);
        float GetMass() const;

        void SetGravityEnabled(bool value);
        bool GetGravityEnabled() const;

        void SetMaxLinearVelocity(float maxLinearVelocity);
        float GetMaxLinearVelocity() const;

        void SetMaxAngularVelocity(float maxAngularVelocity);
        float GetMaxAngularVelocity() const;

        void SetRotationLock(std::array<bool, 3> value);
        std::array<bool, 3> GetRotationLock() const;

        void SetPositionLock(std::array<bool, 3> value);
        std::array<bool, 3> GetPositionLock() const;

        bool IsColliderAttached(const Collider *collider) const;

        void OnPrePhysicsUpdate() override;
        void OnPostPhysicsUpdate() override;

        void OnCopied() override;
        void OnDestroyed() override;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };


}
