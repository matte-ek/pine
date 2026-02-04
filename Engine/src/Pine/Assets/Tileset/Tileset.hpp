#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Graphics/TextureAtlas/TextureAtlas.hpp"

namespace Pine
{

    enum TileFlags
    {
        TileFlags_NoCollision = (1 << 0),
        TileFlags_Hidden = (1 << 1),
        TileFlags_Custom1 = (1 << 2),
        TileFlags_Custom2 = (1 << 3),
        TileFlags_Custom3 = (1 << 4),
    };

    struct TileData
    {
        // The index that the tile is being identified as within a tilemap
        std::uint32_t m_Index;

        // The index the renderer will use when rendering the tile from the texture atlas.
        std::uint32_t m_RenderIndex;

        // Preset flags for this tile
        std::uint32_t m_DefaultFlags = 0;

        // Collider
        Pine::Vector2f m_ColliderOffset = Pine::Vector2f(0.f);
        Pine::Vector2f m_ColliderSize = Pine::Vector2f(1.f);
        float m_ColliderRotation = 0.f;

        Texture2D* m_Texture;
    };

    class Tileset : public Asset
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

        TileData* AddTile(Texture2D* texture, std::uint32_t defaultFlags = 0);
        void RemoveTile(const TileData& tile);

        TileData* GetTileByIndex(std::uint32_t index);
        TileData* GetTileByTexture(const Texture2D* texture);

        const std::vector<TileData>& GetTileList();

        Graphics::TextureAtlas* GetTextureAtlas() const;

        void Build();

        bool LoadFromFile(AssetLoadStage stage) override;
        bool SaveToFile() override;

        void Dispose() override;
    };

}