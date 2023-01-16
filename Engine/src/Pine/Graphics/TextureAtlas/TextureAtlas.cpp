#include "TextureAtlas.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Renderer2D/Renderer2D.hpp"
#include "Pine/Rendering/RenderingContext.hpp"

Pine::Graphics::TextureAtlas::TextureAtlas(Pine::Vector2i size, int tileSize) :
    m_Size(size),
    m_TileSize(tileSize)
{
    m_AtlasFrameBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_AtlasFrameBuffer->Create(size.x, size.y, Buffers::ColorBuffer);
}

Pine::Graphics::ITexture* Pine::Graphics::TextureAtlas::GetColorBuffer() const
{
    return m_AtlasFrameBuffer->GetColorBuffer();
}

std::uint32_t Pine::Graphics::TextureAtlas::AddTexture(Pine::Graphics::ITexture* texture)
{
    std::uint32_t itemId = m_Textures.size();

    m_Textures.push_back(texture);
    m_TextureUvOffsets.push_back(CalculateTextureUv(itemId));

    return itemId;
}

const Pine::Vector2f& Pine::Graphics::TextureAtlas::GetTextureUvOffset(std::uint32_t itemId) const
{
    return m_TextureUvOffsets[itemId];
}

Pine::Vector2f Pine::Graphics::TextureAtlas::CalculateTextureUv(std::uint32_t itemIndex) const
{
    const auto itemsPerRow = static_cast<int>(std::floor(m_Size.x / m_TileSize));
    const auto row = static_cast<float>(std::floor(static_cast<float>(itemIndex) / static_cast<float>(itemsPerRow)));
    const auto column = static_cast<float>(itemIndex % itemsPerRow);

    const auto tileUvOffset = Vector2f(static_cast<float>(m_TileSize) / static_cast<float>(m_Size.x));

    return {tileUvOffset.x * column, 1.f - tileUvOffset.y * row};
}

void Pine::Graphics::TextureAtlas::Update()
{
    RenderingContext renderingContext = { m_Size };

    m_AtlasFrameBuffer->Bind();

    Graphics::GetGraphicsAPI()->ClearColor(Color(0, 0, 0, 0));
    Graphics::GetGraphicsAPI()->ClearBuffers(Buffers::ColorBuffer);

    Renderer2D::PrepareFrame();

    for (int i = 0; i < m_Textures.size();i++)
    {
        auto texture = m_Textures[i];

        const auto itemsPerRow = static_cast<int>(std::floor(m_Size.x / m_TileSize));
        const auto row = static_cast<float>(std::floor(static_cast<float>(i) / static_cast<float>(itemsPerRow)));
        const auto column = static_cast<float>(i % itemsPerRow);

        Renderer2D::AddFilledTexturedRectangle(Vector2f(static_cast<float>(m_TileSize) * column, static_cast<float>(m_TileSize) * row),
                                               Vector2f(static_cast<float>(m_TileSize)),
                                               Color(255, 255, 255, 255),
                                               texture);
    }

    Renderer2D::RenderFrame(&renderingContext);

    Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr);
}

void Pine::Graphics::TextureAtlas::SetTileSize(int tileSize)
{
    m_TileSize = tileSize;
}

int Pine::Graphics::TextureAtlas::GetTileSize() const
{
    return m_TileSize;
}

float Pine::Graphics::TextureAtlas::GetTextureUvScale() const
{
    return static_cast<float>(m_TileSize) / static_cast<float>(m_Size.x);
}

void Pine::Graphics::TextureAtlas::Dispose()
{
    if (m_AtlasFrameBuffer)
        Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_AtlasFrameBuffer);
}

void Pine::Graphics::TextureAtlas::RemoveTexture(std::uint32_t texture)
{
    m_Textures.erase(m_Textures.begin() + static_cast<int>(texture));
    m_TextureUvOffsets.erase(m_TextureUvOffsets.begin() + static_cast<int>(texture));
}

void Pine::Graphics::TextureAtlas::RemoveAllTextures()
{
    m_Textures.clear();
    m_TextureUvOffsets.clear();
}