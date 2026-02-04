#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/Component/Component.hpp"

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

    enum ColliderLayer
    {
        ColliderLayerDefault = (1 << 0)
    };

    class Collider final : public Component
    {
    private:
        ColliderType m_ColliderType = ColliderType::Box;

        Vector3f m_Position = Vector3f(0.f);
        Vector3f m_Size = Vector3f(1.f);

        physx::PxTransform m_Transform = physx::PxTransform();

        physx::PxRigidStatic* m_CollisionRigidBody = nullptr;

        std::uint32_t m_Layer = ColliderLayerDefault;
        std::uint32_t m_LayerMask = 0xFFFFFFFF;

        bool m_IsTrigger = false;
        std::uint32_t m_TriggerMask = 0xFFFFFFFF;

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

        void SetLayer(std::uint32_t layer);
        std::uint32_t GetLayer() const;

        void SetLayerMask(std::uint32_t includeLayers);
        std::uint32_t GetLayerMask() const;

        void SetIsTrigger(bool isTrigger);
        bool IsTrigger() const;

        void SetTriggerMask(std::uint32_t mask);
        std::uint32_t GetTriggerMask() const;

        physx::PxShape* CreateCollisionShape() const;
        physx::PxFilterData GetFilterData() const;

        void Reset();

        void OnPrePhysicsUpdate() override;
        void OnDestroyed() override;
        void OnCopied() override;

        void LoadData(const nlohmann::json &j) override;
        void SaveData(nlohmann::json &j) override;
    };

}