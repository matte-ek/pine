#include "TilemapRenderer.hpp"

Pine::TilemapRenderer::TilemapRenderer() :
      IComponent(ComponentType::TilemapRenderer)
{
}

void Pine::TilemapRenderer::SetTilemap(Pine::Tilemap* map)
{
    m_Tilemap = map;
}

Pine::Tilemap* Pine::TilemapRenderer::GetTilemap() const
{
    return m_Tilemap.Get();
}

void Pine::TilemapRenderer::SetOrder(int order)
{
    m_Order = order;
}

int Pine::TilemapRenderer::GetOrder() const
{
    return m_Order;
}
