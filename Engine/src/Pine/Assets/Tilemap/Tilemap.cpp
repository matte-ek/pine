#include "Tilemap.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::Tilemap::Tilemap()
{
    m_Type = AssetType::Tilemap;
    m_LoadMode = AssetLoadMode::MultiThread;
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
    m_State = AssetState::Unloaded;
}

void Pine::Tilemap::CreateTile(std::uint32_t index, Pine::Vector2i gridPosition, std::uint32_t flags)
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
    tileInstance.m_Flags = tileData->m_DefaultFlags;

    if (flags != 0)
        tileInstance.m_Flags = flags;

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

bool Pine::Tilemap::LoadFromFile(AssetLoadStage stage)
{
    auto jsonOpt = Serialization::LoadFromFile(m_FilePath);

    if (!jsonOpt.has_value())
        return false;

    const auto j = jsonOpt.value();

    Serialization::LoadAsset(j, "tileSet", m_Tileset, false);

    if (j.contains("tiles"))
    {
        for (const auto& tileData : j["tiles"])
        {
            CreateTile(tileData["i"].get<std::uint32_t>(), Vector2i(tileData["x"].get<int>(), tileData["y"].get<int>()), tileData.contains("f") ? tileData["f"].get<std::uint32_t>() : 0);
        }
    }

    m_State = AssetState::Loaded;

    return true;
}

bool Pine::Tilemap::SaveToFile()
{
    nlohmann::json j;

    j["tileSet"] = Serialization::StoreAsset(m_Tileset.Get());

    for (const auto& tileInstance : m_Tiles)
    {
        auto tileData = m_Tileset->GetTileByIndex(tileInstance.m_Index);

        if (tileData == nullptr)
        {
            continue;
        }

        nlohmann::json tileInstanceData;

        tileInstanceData["i"] = tileInstance.m_Index;
        tileInstanceData["x"] = tileInstance.m_Position.x;
        tileInstanceData["y"] = tileInstance.m_Position.y;

        if (tileInstance.m_Flags != tileData->m_DefaultFlags)
        {
            tileInstanceData["f"] = tileInstance.m_Flags;
        }

        j["tiles"].push_back(tileInstanceData);
    }

    Serialization::SaveToFile(m_FilePath, j);

    if (m_Tileset.Get())
    {
        m_HasDependencies = true;
        m_DependencyFiles.push_back(m_Tileset->GetFilePath().string());
    }

    return true;
}