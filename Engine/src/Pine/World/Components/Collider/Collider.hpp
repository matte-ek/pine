#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include <reactphysics3d/reactphysics3d.h>

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

    class Collider : public IComponent
    {
    private:
        ColliderType m_ColliderType = ColliderType::Box;

        reactphysics3d::CollisionShape* m_CollisionShape = nullptr;

        Vector3f m_Position = Vector3f(0.f);
        Vector3f m_Size = Vector3f(1.f);

        reactphysics3d::Transform m_CollisionBodyTransform;
        reactphysics3d::Transform m_CollisionTransform;

        reactphysics3d::Collider* m_Collider = nullptr;
        reactphysics3d::CollisionBody* m_CollisionBody = nullptr;

        bool m_ShapeUpdated = false;

        void UpdateBody();

        void CreateShape();
        void DisposeShape();
        void UpdateShape();

        reactphysics3d::TriangleMesh* LoadTriangleMesh();
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

        bool PollShapeUpdated();

        reactphysics3d::CollisionShape* GetCollisionShape() const;
        reactphysics3d::Collider* GetCollider() const;

        void OnPrePhysicsUpdate() override;
        void OnDestroyed() override;
        void OnCopied() override;

        void LoadData(const nlohmann::json &j) override;
        void SaveData(nlohmann::json &j) override;
    };

}