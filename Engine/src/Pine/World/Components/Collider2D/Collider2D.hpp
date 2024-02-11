#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"

namespace Pine
{

    enum class Collider2DType
    {
        Sprite,
        Box,
        Tilemap
    };

    class Collider2D final : public IComponent
    {
        Collider2DType m_ColliderType = Collider2DType::Box;

        Vector2f m_ColliderOffset = Vector2f(0.f);
        Vector2f m_ColliderSize = Vector2f(1.f);
    public:
        explicit Collider2D();

        void SetColliderType(Collider2DType type);
        Collider2DType GetColliderType() const;

        void SetOffset(Vector2f offset);
        const Vector2f & GetOffset() const;

        void SetSize(Vector2f size);
        const Vector2f & GetSize() const;

        void OnRender(float deltaTime) override;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}