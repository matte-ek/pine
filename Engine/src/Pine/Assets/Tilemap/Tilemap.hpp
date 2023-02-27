#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"

namespace Pine
{

    enum PineTileFlags : std::uint32_t
    {
        NoCollison = (1 << 0), 
        NoRender = (1 << 1)
    };

    struct TileInstance // (Not to be confused with TileData)
    {
        // The index that the tile is being identified as within a tilemap
        std::uint32_t m_Index = 0;
        
        // The index the renderer will use when rendering the tile from the texture atlas.
        std::uint32_t m_RenderIndex = 0;

        // Flags for the tile, can be modified per tile but will by default get set by the preset from the tileset.
        // Custom flags may be used, as long as they don't collide with the engine flags specified in PineTileFlags
        std::uint32_t m_Flags = 0;

        // XY position for this tile in a grid, tile size within the grid can be obtained from the tileset itself.
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

        void CreateTile(std::uint32_t index, Vector2i gridPosition, std::uint32_t flags = 0);
        void RemoveTile(const TileInstance& instance);

        TileInstance const* GetTileByPosition(Vector2i gridPosition) const;

        const std::vector<TileInstance>& GetTiles() const;

        bool LoadFromFile(AssetLoadStage stage = AssetLoadStage::Default) override;
        bool SaveToFile() override;

        void Dispose() override;
    };

}