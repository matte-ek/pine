#pragma once

#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"

namespace Pine
{

    enum class SpriteScalingMode
    {
        Stretch,
        Repeat
    };

    class SpriteRenderer final : public IComponent
    {
    private:
        AssetHandle<Texture2D> m_StaticTexture;

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

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}