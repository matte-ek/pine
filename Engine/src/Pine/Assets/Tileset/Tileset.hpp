#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Graphics/TextureAtlas/TextureAtlas.hpp"

namespace Pine
{

    struct TileData
    {
        // The index that the tile is being identified as within a tilemap
        std::uint32_t m_Index;

        // The index the renderer will use when rendering the tile from the texture atlas.
        std::uint32_t m_RenderIndex;

        // Preset flags for this tile
        std::uint32_t m_DefaultFlags = 0;

        Texture2D* m_Texture;
    };

    class Tileset : public IAsset
    {
    private:
        Graphics::TextureAtlas* m_TextureAtlas = nullptr;
        int m_TileSize = -1;

        std::vector<TileData> m_Tiles;

        std::uint32_t m_CurrentTileIndex = 0;
    public:
        Tileset();

        void SetTileSize(int size);
        int GetTileSize() const;

        void AddTile(Texture2D* texture, std::uint32_t defaultFlags = 0);
        void RemoveTile(const TileData& tile);

        TileData const* GetTileByIndex(std::uint32_t index) const;
        TileData const* GetTileByTexture(const Texture2D* texture) const;

        const std::vector<TileData>& GetTileList();

        Graphics::TextureAtlas* GetTextureAtlas() const;

        void Build();

        bool LoadFromFile(AssetLoadStage stage) override;
        bool SaveToFile() override;

        void Dispose() override;
    };

}