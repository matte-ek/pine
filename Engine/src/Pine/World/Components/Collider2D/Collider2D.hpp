#pragma once

#include <box2d/b2_math.h>

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/Component/Component.hpp"

class b2Fixture;
class b2Body;

namespace Pine
{

    enum class Collider2DType
    {
        Box,
        Sprite,
        Tilemap
    };

    class Collider2D final : public Component
    {
    private:
        Collider2DType m_ColliderType = Collider2DType::Box;

        Vector2f m_ColliderOffset = Vector2f(0.f);
        Vector2f m_ColliderSize = Vector2f(1.f);
        float m_ColliderRotation = 0.f;

        b2Body* m_Body = nullptr;
        b2Fixture* m_Fixture = nullptr;

        // Since you cannot resize the body, we need to keep track of the size during
        // it's creation and update it when the user size changes.
        Vector2f m_BodySize = Vector2f(1.f);

        void UpdateBody();
    protected:
        Vector2f ComputePosition() const;
        Vector2f ComputeSize() const;
        float ComputeRotation() const;
    public:
        explicit Collider2D();

        void SetColliderType(Collider2DType type);
        Collider2DType GetColliderType() const;

        void SetOffset(Vector2f offset);
        const Vector2f & GetOffset() const;

        void SetSize(Vector2f size);
        const Vector2f & GetSize() const;

        void SetRotation(float rotation);
        float GetRotation() const;

        void OnPrePhysicsUpdate() override;
        void OnDestroyed() override;
        void OnCopied() override;

        void OnRender(float deltaTime) override;
        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;

        friend class RigidBody2D;
    };

}