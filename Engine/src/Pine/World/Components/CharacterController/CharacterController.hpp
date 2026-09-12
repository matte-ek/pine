#pragma once

#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

namespace physx
{
    class PxController;
}

namespace Pine
{
    // A kinematic, collide-and-slide capsule character controller backed by PhysX's PxController.
    // Unlike a RigidBody it is driven by intent (a desired displacement) rather than forces:
    // it slides along walls, steps over small ledges, respects a slope limit and reports whether
    // it is grounded. Put it on any entity that should walk the world (player, NPC, ...); the game
    // decides how to move it by calling Move() from a script.
    class CharacterController final : public Component
    {
    private:
        // Serialized capsule + movement configuration.
        float m_Radius = 0.3f;         // Capsule radius.
        float m_Height = 1.2f;         // Height of the cylindrical section (excludes the two caps).
        float m_SlopeLimit = 45.0f;    // Maximum walkable slope, in degrees.
        float m_StepOffset = 0.3f;     // Maximum step height the controller auto-climbs.
        float m_ContactOffset = 0.1f;  // Skin width kept between the capsule and geometry.
        float m_Gravity = -9.81f;      // Downward acceleration applied while airborne.

        // Collision layer/mask this controller uses (mirrors Collider's defaults).
        std::uint32_t m_Layer = 1;              // 1 << 0, the default collision layer.
        std::uint32_t m_LayerMask = 0xFFFFFFFF; // Collide with everything by default.

        // Runtime state (not serialized).
        physx::PxController* m_Controller = nullptr;
        Vector3f m_PendingMovement = Vector3f(0.f); // World-space displacement queued from script.
        float m_VerticalVelocity = 0.f;
        bool m_Grounded = false;
        bool m_StaticWarningIssued = false; // So the static-entity warning is logged once, not per tick.

        void CreateController();
        void ApplyFilterData() const;
        void WriteBackTransform() const;
        void SetTransformFromWorldPosition(const Vector3f& worldPosition) const;

        struct CharacterControllerSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Radius, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Height, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(SlopeLimit, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(StepOffset, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(ContactOffset, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Gravity, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Layer, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(LayerMask, Serialization::DataType::Int32);
        };
    public:
        CharacterController();

        // Queue a world-space displacement to apply on the next physics tick. Mirrors Unity's
        // CharacterController.Move() - the argument is a displacement, not a velocity. Gravity is
        // applied by the controller itself, so callers only need to supply the intended movement.
        void Move(const Vector3f& motion);

        // Teleport the controller to a world-space position (the entity origin, i.e. the capsule's
        // feet). Use this rather than writing the Transform: the controller owns its position and
        // overwrites the Transform every physics tick, so a Transform-only move is undone on the
        // next tick. Clears any queued movement and resets the accumulated fall speed.
        void SetPosition(const Vector3f& position);

        bool IsGrounded() const;

        void SetRadius(float radius);
        float GetRadius() const;

        void SetHeight(float height);
        float GetHeight() const;

        void SetSlopeLimit(float degrees);
        float GetSlopeLimit() const;

        void SetStepOffset(float stepOffset);
        float GetStepOffset() const;

        void SetContactOffset(float contactOffset);
        float GetContactOffset() const;

        void SetGravity(float gravity);
        float GetGravity() const;

        // The collision layer this controller occupies, and the mask of layers it collides with.
        // Same layer space as Collider, so the two can be configured against each other.
        void SetLayer(std::uint32_t layer);
        std::uint32_t GetLayer() const;

        void SetLayerMask(std::uint32_t layerMask);
        std::uint32_t GetLayerMask() const;

        // Driven by Physics3D once per fixed physics tick, before the scene simulates.
        void Simulate(float elapsedTime);

        void OnCopied() override;
        void OnDestroyed() override;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };
}
