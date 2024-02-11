#pragma once

#include "Pine/World/Components/IComponent/IComponent.hpp"

#include <reactphysics3d/reactphysics3d.h>

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

        reactphysics3d::RigidBody *m_RigidBody = nullptr;

        reactphysics3d::Transform m_RigidBodyTransform;
        reactphysics3d::Transform m_ColliderTransform;

        std::array<bool, 3> m_RotationLock = {false, false, false};

        reactphysics3d::Collider *m_Collider = nullptr;
        Pine::Collider *m_EngineCollider = nullptr;

        void UpdateColliders();

        void UpdateRigidBodyProperties();

    public:
        RigidBody();

        reactphysics3d::RigidBody *GetRigidBody() const;

        void SetRigidBodyType(RigidBodyType type);
        RigidBodyType GetRigidBodyType() const;

        void SetMass(float mass);
        float GetMass() const;

        void SetGravityEnabled(bool value);
        bool GetGravityEnabled() const;

        void SetRotationLock(std::array<bool, 3> rot);
        const std::array<bool, 3>& GetRotationLock() const;

        void DetachCollider();

        bool IsColliderAttached(Collider *collider) const;

        void OnPrePhysicsUpdate() override;
        void OnPostPhysicsUpdate() override;

        void OnCopied() override;

        void LoadData(const nlohmann::json &j) override;
        void SaveData(nlohmann::json &j) override;
    };


}