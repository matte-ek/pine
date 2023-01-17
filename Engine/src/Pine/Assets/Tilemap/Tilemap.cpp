#include "Tilemap.hpp"

Pine::Tilemap::Tilemap()
{
    m_Type = AssetType::Tilemap;
}

void Pine::Tilemap::SetTileset(Pine::Tileset* tileset)
{
    m_Tileset = tileset;
}

Pine::Tileset* Pine::Tilemap::GetTileset() const
{
    return m_Tileset.Get();
}

const std::vector<Pine::TileInstance>& Pine::Tilemap::GetTiles() const
{
    return m_Tiles;
}

void Pine::Tilemap::Dispose()
{
}

void Pine::Tilemap::CreateTile(std::uint32_t index, Pine::Vector2i gridPosition)
{
    if (m_Tileset == nullptr)
    {
        throw std::runtime_error("Attempt to add tiles without tileset present.");
    }

    auto tileData = m_Tileset->GetTileByIndex(index);

    if (tileData == nullptr)
    {
        return;
    }

    TileInstance tileInstance;

    tileInstance.m_Index = index;
    tileInstance.m_Position = gridPosition;
    tileInstance.m_RenderIndex = tileData->m_RenderIndex;

    m_Tiles.push_back(tileInstance);
}

void Pine::Tilemap::RemoveTile(const Pine::TileInstance& instance)
{
    for (int i = 0; i < m_Tiles.size();i++)
    {
        if (&m_Tiles[i] == &instance)
        {
            m_Tiles.erase(m_Tiles.begin() + i);
            break;
        }
    }
}

const Pine::TileInstance* Pine::Tilemap::GetTileByPosition(Pine::Vector2i gridPosition) const
{
    for (const auto & m_Tile : m_Tiles)
    {
        if (m_Tile.m_Position == gridPosition)
        {
            return &m_Tile;
        }
    }

    return nullptr;
}
