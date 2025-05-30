#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"

#include "physx/PxPhysicsAPI.h"

namespace Pine
{

    enum class ColliderType
    {
        Box,
        Sphere,
        Capsule,
        ConvexMesh,
        ConcaveMesh,
        HeightField
    };

    class Collider final : public IComponent
    {
    private:
        ColliderType m_ColliderType = ColliderType::Box;

        Vector3f m_Position = Vector3f(0.f);
        Vector3f m_Size = Vector3f(1.f);

        physx::PxTransform m_Transform = physx::PxTransform();

        physx::PxRigidStatic* m_CollisionRigidBody = nullptr;

        void UpdateBody();
    public:
        Collider();

        void SetColliderType(ColliderType type);
        ColliderType GetColliderType() const;

        void SetPosition(Vector3f position);
        const Vector3f& GetPosition() const;

        void SetSize(Vector3f size);
        const Vector3f& GetSize() const;

        // Used for sphere and capsule
        void SetRadius(float radius);
        float GetRadius() const;

        // Used for capsule only
        void SetHeight(float height);
        float GetHeight() const;

        void Reset();

        physx::PxShape* CreateCollisionShape() const;
        physx::PxRigidStatic* GetCollider() const;

        void OnPrePhysicsUpdate() override;
        void OnDestroyed() override;
        void OnCopied() override;

        void LoadData(const nlohmann::json &j) override;
        void SaveData(nlohmann::json &j) override;
    };

}