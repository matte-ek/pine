#include "Tileset.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "../../Core/Serialization/Json/SerializationJson.hpp"

constexpr int TextureAtlasSize = 512;

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

Pine::TileData* Pine::Tileset::AddTile(Texture2D* texture, std::uint32_t defaultFlags)
{
    TileData tile{};

    tile.m_Texture = texture;
    tile.m_Index = m_CurrentTileIndex++;
    tile.m_DefaultFlags = defaultFlags;

    m_HasBeenModified = true;

    m_Tiles.push_back(tile);

    return &m_Tiles[m_Tiles.size() - 1];
}

void Pine::Tileset::RemoveTile(const TileData& tile)
{
    for (int i = 0; i < m_Tiles.size();i++)
    {
        if (m_Tiles[i].m_Index == tile.m_Index)
        {
            m_Tiles.erase(m_Tiles.begin() + i);
            m_HasBeenModified = true;
            break;
        }
    }
}

Pine::TileData* Pine::Tileset::GetTileByIndex(std::uint32_t index)
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

Pine::TileData* Pine::Tileset::GetTileByTexture(const Texture2D* texture)
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
    const auto jsonOpt = SerializationJson::LoadFromFile(m_FilePath);

    if (!jsonOpt.has_value())
    {
        return false;
    }

    const auto j = jsonOpt.value();

    SerializationJson::LoadValue(j, "tileSize", m_TileSize);

    if (j.contains("tiles"))
    {
        for (const auto& tileData : j["tiles"])
        {
            std::uint32_t flags = 0;

            if (tileData.contains("flags"))
                flags = tileData["flags"].get<std::uint32_t>();

            auto tileTexture = Pine::Assets::Get<Texture2D>(tileData["texture"]);

            if (!tileTexture)
            {
                Log::Warning("Could not find tile texture " + tileData["texture"].get<std::string>());

                // We do this to avoid fucking up the indices for other future tiles that may be present.
                m_CurrentTileIndex++;

                continue;
            }

            auto tile = AddTile(tileTexture, flags);

            SerializationJson::LoadVector2(tileData, "offset", tile->m_ColliderOffset);
            SerializationJson::LoadVector2(tileData, "size", tile->m_ColliderSize);

            if (tileData.contains("rotation"))
                tile->m_ColliderRotation = tileData["rotation"];
        }
    }

    Build();

    m_State = AssetState::Loaded;

    return true;
}

bool Pine::Tileset::SaveToFile()
{
    nlohmann::json j;

    j["tileSize"] = m_TileSize;

    for (const auto& tile : m_Tiles)
    {
        nlohmann::json tileJson;

        tileJson["texture"] = tile.m_Texture->GetPath();

        if (tile.m_DefaultFlags != 0)
            tileJson["flags"] = tile.m_DefaultFlags;

        if (tile.m_ColliderOffset != Pine::Vector2f(0.f))
            tileJson["offset"] = SerializationJson::StoreVector2(tile.m_ColliderOffset);

        if (tile.m_ColliderSize != Pine::Vector2f(1.f))
            tileJson["size"] = SerializationJson::StoreVector2(tile.m_ColliderSize);

        if (tile.m_ColliderRotation != 0.f)
            tileJson["rotation"] = tile.m_ColliderRotation;

        j["tiles"].push_back(tileJson);
    }

    SerializationJson::SaveToFile(m_FilePath, j);

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

    m_HasBeenModified = false;

    return true;
}