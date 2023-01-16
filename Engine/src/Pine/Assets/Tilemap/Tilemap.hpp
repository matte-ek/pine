#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"

namespace Pine
{

    struct TileInstance
    {
        std::uint32_t m_Index = 0;

        std::uint32_t m_RenderIndex = 0;

        Vector2i m_Position = Vector2i(0);
    };

    class Tilemap : public IAsset
    {
    private:
        AssetContainer<Tileset> m_Tileset;

        std::vector<TileInstance> m_Tiles;
    public:
        Tilemap();

        void SetTileset(Tileset* tileset);
        Tileset* GetTileset() const;

        void CreateTile(std::uint32_t index, Vector2i gridPosition);
        void RemoveTile(const TileInstance& instance);

        TileInstance const* GetTileByPosition(Vector2i gridPosition) const;

        const std::vector<TileInstance>& GetTiles() const;

        void Dispose() override;
    };

}