#pragma once

#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"

#include <vector>

namespace Pine::Graphics
{

    class TextureAtlas
    {
    private:
        IFrameBuffer* m_AtlasFrameBuffer = nullptr;

        std::vector<ITexture*> m_Textures;
        std::vector<Vector2f> m_TextureUvOffsets;

        int m_TileSize;
        Vector2i m_Size;

        Vector2f CalculateTextureUv(std::uint32_t itemIndex) const;
    public:
        TextureAtlas(Vector2i size, int tileSize);

        int GetTileSize() const;

        const Vector2f& GetTextureUvOffset(std::uint32_t itemId) const;
        float GetTextureUvScale() const;

        ITexture* GetColorBuffer() const;

        std::uint32_t AddTexture(ITexture* texture);

        void Update();
    };

}