#include "Tileset.hpp"

constexpr int TextureAtlasSize = 1024;

Pine::Tileset::Tileset()
{
    m_Type = AssetType::Tileset;
}

void Pine::Tileset::Dispose()
{
    if (m_TextureAtlas)
    {
        m_TextureAtlas->Dispose();

        delete m_TextureAtlas;

        m_TextureAtlas = nullptr;
    }
}

void Pine::Tileset::Build()
{
    if (m_Tiles.empty())
    {
        return;
    }

    const int tileSize = m_TileSize == -1 ? m_Tiles[0].m_Texture->GetWidth() : m_TileSize;

    m_TileSize = tileSize;

    if (!m_TextureAtlas)
    {
        m_TextureAtlas = new Graphics::TextureAtlas(Vector2i(TextureAtlasSize), tileSize);
    }

    m_TextureAtlas->RemoveAllTextures();
    m_TextureAtlas->SetTileSize(tileSize);

    for (auto& tile : m_Tiles)
    {
        tile.m_RenderIndex = m_TextureAtlas->AddTexture(tile.m_Texture->GetGraphicsTexture());
    }

    m_TextureAtlas->Update();
}

Pine::Graphics::TextureAtlas* Pine::Tileset::GetTextureAtlas() const
{
    return m_TextureAtlas;
}

void Pine::Tileset::AddTile(Pine::Texture2D* texture)
{
    TileData tile{};

    tile.m_Texture = texture;
    tile.m_Index = m_CurrentTileIndex++;

    m_Tiles.push_back(tile);
}

void Pine::Tileset::RemoveTile(const Pine::TileData& tile)
{
    for (int i = 0; i < m_Tiles.size();i++)
    {
        if (m_Tiles[i].m_Index == tile.m_Index)
        {
            m_Tiles.erase(m_Tiles.begin() + i);
            break;
        }
    }
}

Pine::TileData const* Pine::Tileset::GetTileByIndex(std::uint32_t index) const
{
    for (auto& tile : m_Tiles)
    {
        if (tile.m_Index == index)
        {
            return &tile;
        }
    }

    return nullptr;
}

Pine::TileData const* Pine::Tileset::GetTileByTexture(Pine::Texture2D* texture) const
{
    for (auto& tile : m_Tiles)
    {
        if (tile.m_Texture == texture)
        {
            return &tile;
        }
    }

    return nullptr;
}

const std::vector<Pine::TileData>& Pine::Tileset::GetTileList()
{
    return m_Tiles;
}

void Pine::Tileset::SetTileSize(int size)
{
    m_TileSize = size;
}

int Pine::Tileset::GetTileSize() const
{
    return m_TileSize;
}
