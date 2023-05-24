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

    class Collider2D : public IComponent
    {
        Collider2DType m_ColliderType = Collider2DType::Box;

        Pine::Vector2f m_ColliderOffset = Vector2f(0.f);
        Pine::Vector2f m_ColliderSize = Vector2f(1.f);
    public:
        explicit Collider2D();

        void SetColliderType(Collider2DType type);
        Collider2DType GetColliderType() const;

        void SetOffset(Pine::Vector2f offset);
        const Pine::Vector2f& GetOffset() const;

        void SetSize(Pine::Vector2f size);
        const Pine::Vector2f& GetSize() const;

        void OnRender(float deltaTime) override;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}