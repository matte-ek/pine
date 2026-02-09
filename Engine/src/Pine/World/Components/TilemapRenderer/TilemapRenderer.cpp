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

void Pine::TilemapRenderer::LoadData(const nlohmann::json& j)
{
    SerializationJson::LoadAsset(j, "tm", m_Tilemap);
    SerializationJson::LoadValue(j, "odr", m_Order);
}

void Pine::TilemapRenderer::SaveData(nlohmann::json& j)
{
    j["tm"] = SerializationJson::StoreAsset(m_Tilemap.Get());
    j["odr"] = m_Order;
}
