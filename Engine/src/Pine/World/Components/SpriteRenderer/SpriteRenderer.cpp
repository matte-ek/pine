#include "SpriteRenderer.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::SpriteRenderer::SpriteRenderer() :
      IComponent(ComponentType::SpriteRenderer)
{
}

void Pine::SpriteRenderer::SetTexture(Pine::Texture2D* texture)
{
    m_StaticTexture = texture;
}

Pine::Texture2D* Pine::SpriteRenderer::GetTexture() const
{
    return m_StaticTexture.Get();
}

void Pine::SpriteRenderer::SetOrder(int order)
{
    m_Order = order;
}

int Pine::SpriteRenderer::GetOrder() const
{
    return m_Order;
}

void Pine::SpriteRenderer::SetScalingMode(Pine::SpriteScalingMode scalingMode)
{
    m_ScalingMode = scalingMode;
}

Pine::SpriteScalingMode Pine::SpriteRenderer::GetScalingMode() const
{
    return m_ScalingMode;
}

void Pine::SpriteRenderer::LoadData(const nlohmann::json& j)
{
    Serialization::LoadAsset(j, "tex", m_StaticTexture);
    Serialization::LoadValue(j, "scl", m_ScalingMode);
    Serialization::LoadValue(j, "odr", m_Order);
}

void Pine::SpriteRenderer::SaveData(nlohmann::json& j)
{
    j["tex"] = Serialization::StoreAsset(m_StaticTexture.Get());
    j["scl"] = m_ScalingMode;
    j["odr"] = m_Order;
}
