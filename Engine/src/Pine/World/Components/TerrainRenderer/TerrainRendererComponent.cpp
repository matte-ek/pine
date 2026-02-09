#include "TerrainRendererComponent.hpp"

#include "../../../Core/Serialization/Json/SerializationJson.hpp"

Pine::TerrainRendererComponent::TerrainRendererComponent() :
    Component(ComponentType::TerrainRenderer)
{
}

void Pine::TerrainRendererComponent::SetTerrain(Terrain* terrain)
{
    m_Terrain = terrain;
}

Pine::Terrain* Pine::TerrainRendererComponent::GetTerrain() const
{
    return m_Terrain.Get();
}

void Pine::TerrainRendererComponent::LoadData(const nlohmann::json& j)
{
    SerializationJson::LoadAsset(j, "ter", m_Terrain);
}

void Pine::TerrainRendererComponent::SaveData(nlohmann::json& j)
{
    j["ter"] = SerializationJson::StoreAsset(m_Terrain.Get());
}
