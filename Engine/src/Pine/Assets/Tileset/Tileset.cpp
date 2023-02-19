#include "Tileset.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

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

    m_State = AssetState::Unloaded;
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

void Pine::Tileset::AddTile(Pine::Texture2D* texture, std::uint32_t defaultFlags)
{
    TileData tile{};

    tile.m_Texture = texture;
    tile.m_Index = m_CurrentTileIndex++;
    tile.m_DefaultFlags = defaultFlags;

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

bool Pine::Tileset::LoadFromFile(AssetLoadStage stage)
{
    const auto jsonOpt = Serialization::LoadFromFile(m_FilePath);

    if (!jsonOpt.has_value())
    {
        return false;
    }

    const auto j = jsonOpt.value();

    Serialization::LoadValue(j, "tileSize", m_TileSize);

    for (const auto& tileData : j["tiles"])
    {
        std::uint32_t flags = 0;

        if (tileData.contains("flags"))
            flags = tileData["flags"].get<std::uint32_t>();

        Pine::Texture2D* tileTexture = Pine::Assets::GetAsset<Pine::Texture2D>(tileData["texture"]);

        if (!tileTexture)
        {
            Log::Warning("Could not find tile texture " + tileData["texture"].get<std::string>());
            
            // We do this to avoid fucking up the indices for other future tiles that may be present.
            m_CurrentTileIndex++;

            continue;
        }

        AddTile(tileTexture, flags);
    }

    Build();

    return true;
}

bool Pine::Tileset::SaveToFile()
{
    nlohmann::json j;

    j["tileSize"] = m_TileSize;

    for (const auto& tile : m_Tiles)
    {
        nlohmann::json tileJson;

        tileJson["texture"] = tile.m_Texture->GetFilePath().string();

        if (tile.m_DefaultFlags != 0)
            tileJson["flags"] = tile.m_DefaultFlags;

        j["tiles"].push_back(tileJson);
    }

    Serialization::SaveToFile(m_FilePath, j);

    if (!m_Tiles.empty())
    {
        m_HasDependencies = true;
        
        for (const auto& tile : m_Tiles)
        {
            if (!tile.m_Texture)
                continue;

            m_DependencyFiles.push_back(tile.m_Texture->GetFilePath().string());
        }
    }

    return true;
}