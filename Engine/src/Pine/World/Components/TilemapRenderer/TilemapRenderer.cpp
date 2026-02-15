#include "TilemapRenderer.hpp"
#include "../../../Core/Serialization/Json/SerializationJson.hpp"

Pine::TilemapRenderer::TilemapRenderer() :
      Component(ComponentType::TilemapRenderer)
{
}

void Pine::TilemapRenderer::SetTilemap(Tilemap* map)
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

void Pine::TilemapRenderer::LoadData(const ByteSpan& span)
{
    TilemapSerializer serializer;

    serializer.Read(span);

    serializer.Tilemap.Read(m_Tilemap);
    serializer.Order.Read(m_Order);
}

Pine::ByteSpan Pine::TilemapRenderer::SaveData()
{
    TilemapSerializer serializer;

    serializer.Tilemap.Write(m_Tilemap);
    serializer.Order.Write(m_Order);

    return serializer.Write();
}