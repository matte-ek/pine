#include "SpriteRenderer.hpp"
#include "../../../Core/Serialization/Json/SerializationJson.hpp"

Pine::SpriteRenderer::SpriteRenderer() :
      Component(ComponentType::SpriteRenderer)
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

void Pine::SpriteRenderer::LoadData(const ByteSpan& span)
{
    SpriteRendererSerializer serializer;

    serializer.Read(span);

    serializer.StaticTexture.Read(m_StaticTexture);
    serializer.Color.Read(m_Color);
    serializer.ScalingMode.Read(m_ScalingMode);
    serializer.Order.Read(m_Order);
}

Pine::ByteSpan Pine::SpriteRenderer::SaveData()
{
    SpriteRendererSerializer serializer;

    serializer.StaticTexture.Write(m_StaticTexture);
    serializer.Color.Write(m_Color);
    serializer.ScalingMode.Write(m_ScalingMode);
    serializer.Order.Write(m_Order);

    return serializer.Write();
}

void Pine::SpriteRenderer::SetScalingMode(SpriteScalingMode scalingMode)
{
    m_ScalingMode = scalingMode;
}

Pine::SpriteScalingMode Pine::SpriteRenderer::GetScalingMode() const
{
    return m_ScalingMode;
}