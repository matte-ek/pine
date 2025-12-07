#pragma once

#include <array>

#include "Pine/World/Components/IComponent/IComponent.hpp"

#include "physx/PxPhysicsAPI.h"

namespace Pine
{
    enum class RigidBodyType
    {
        Static,
        Kinematic,
        Dynamic
    };

    class Collider;

    class RigidBody final : public IComponent
    {
    private:
        RigidBodyType m_RigidBodyType = RigidBodyType::Dynamic;

        float m_Mass = 1.0f;

        bool m_GravityEnabled = true;

        physx::PxRigidDynamic *m_RigidBody = nullptr;
        physx::PxTransform m_RigidBodyTransform;

        std::array<bool, 3> m_RotationLock = {false, false, false};

        Collider *m_EngineCollider = nullptr;

        void UpdateColliders();
        void UpdateBody();
    public:
        RigidBody();

        physx::PxRigidDynamic *GetRigidBody() const;

        void SetRigidBodyType(RigidBodyType type);
        RigidBodyType GetRigidBodyType() const;

        void SetMass(float mass);
        float GetMass() const;

        void SetGravityEnabled(bool value);
        bool GetGravityEnabled() const;

        bool IsColliderAttached(const Collider *collider) const;

        void OnPrePhysicsUpdate() override;
        void OnPostPhysicsUpdate() override;

        void OnCopied() override;
        void OnDestroyed() override;

        void LoadData(const nlohmann::json &j) override;
        void SaveData(nlohmann::json &j) override;
    };


}