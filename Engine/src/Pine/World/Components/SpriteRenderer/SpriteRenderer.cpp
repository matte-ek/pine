#include "SpriteRenderer.hpp"

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
