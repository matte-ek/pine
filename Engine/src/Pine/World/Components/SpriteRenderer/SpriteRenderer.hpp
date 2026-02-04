#pragma once

#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{

    enum class SpriteScalingMode
    {
        Stretch,
        Repeat
    };

    class SpriteRenderer final : public Component
    {
    private:
        AssetHandle<Texture2D> m_StaticTexture;

        Pine::Vector4f m_Color = Pine::Vector4f(1.f);

        SpriteScalingMode m_ScalingMode = SpriteScalingMode::Stretch;

        int m_Order = 0;
    public:
        explicit SpriteRenderer();

        void SetTexture(Texture2D* texture);
        Texture2D* GetTexture() const;

        void SetScalingMode(SpriteScalingMode scalingMode);
        SpriteScalingMode GetScalingMode() const;

        void SetOrder(int order);
        int GetOrder() const;

        void SetColor(const Pine::Vector4f& color);
        const Pine::Vector4f& GetColor() const;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}