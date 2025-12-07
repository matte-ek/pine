#include "SpriteRenderer.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::SpriteRenderer::SpriteRenderer() :
      IComponent(ComponentType::SpriteRenderer)
{
}

void Pine::SpriteRenderer::SetTexture(Texture2D* texture)
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

void Pine::SpriteRenderer::SetColor(const Pine::Vector4f& color)
{
    m_Color = color;
}

const Pine::Vector4f& Pine::SpriteRenderer::GetColor() const
{
    return m_Color;
}

void Pine::SpriteRenderer::SetScalingMode(SpriteScalingMode scalingMode)
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
    Serialization::LoadVector4(j, "clr", m_Color);
}

void Pine::SpriteRenderer::SaveData(nlohmann::json& j)
{
    j["tex"] = Serialization::StoreAsset(m_StaticTexture.Get());
    j["scl"] = m_ScalingMode;
    j["odr"] = m_Order;
    j["clr"] = Serialization::StoreVector4(m_Color);
}
