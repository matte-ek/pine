#include "TilemapRenderer.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

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

void Pine::TilemapRenderer::LoadData(const nlohmann::json& j)
{
    Serialization::LoadAsset(j, "tm", m_Tilemap);
    Serialization::LoadValue(j, "odr", m_Order);
}

void Pine::TilemapRenderer::SaveData(nlohmann::json& j)
{
    j["tm"] = Serialization::StoreAsset(m_Tilemap.Get());
    j["odr"] = m_Order;
}
