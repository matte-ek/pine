#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Component/Component.hpp"

class b2Body;
class b2Fixture;

namespace Pine
{

    enum class RigidBody2DType
    {
        Static,
        Dynamic,
        Kinematic
    };

    class RigidBody2D final : public Component
    {
    private:
        void UpdateBody();

        RigidBody2DType m_RigidBodyType = RigidBody2DType::Dynamic;

        // Since you cannot resize the body, we need to keep track of the size during
        // it's creation and update it when the user size changes.
        Pine::Vector2f m_BodySize = Pine::Vector2f(1.f);

        // Same story for the type, we need to keep track of it during creation.
        RigidBody2DType m_BodyType = RigidBody2DType::Dynamic;

        b2Body* m_Body = nullptr;
        b2Fixture* m_Fixture = nullptr;

        struct RigidBody2DSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Type, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(Size, Serialization::DataType::Vec2);
            PINE_SERIALIZE_PRIMITIVE(BodyType, Serialization::DataType::Int32);
        };
    public:
        RigidBody2D();

        void SetRigidBodyType(RigidBody2DType type);
        RigidBody2DType GetRigidBodyType() const;

        void OnPrePhysicsUpdate() override;
        void OnPostPhysicsUpdate() override;

        void OnDestroyed() override;
        void OnCopied() override;

        void OnRender(float deltaTime) override;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };


}