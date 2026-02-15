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

void Pine::TerrainRendererComponent::LoadData(const ByteSpan& span)
{
    TerrainSerializer terrainSerializer;

    terrainSerializer.Read(span);

    terrainSerializer.Terrain.Read(m_Terrain);
}

Pine::ByteSpan Pine::TerrainRendererComponent::SaveData()
{
    TerrainSerializer terrainSerializer;

    terrainSerializer.Terrain.Write(m_Terrain);

    return terrainSerializer.Write();
}
