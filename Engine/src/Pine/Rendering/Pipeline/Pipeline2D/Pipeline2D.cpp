#include "Pipeline2D.hpp"

#include "Pine/Rendering/Renderer2D/Renderer2D.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/TilemapRenderer/TilemapRenderer.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    enum class RenderItemType
    {
        SpriteRenderer,
        TilemapRenderer
    };

    struct RenderItem
    {
        RenderItemType m_Type = RenderItemType::SpriteRenderer;
        Pine::IComponent* m_ComponentPointer = nullptr;
        int m_Order = 0;
    };

    std::vector<RenderItem> m_RenderItems;
}

void Pine::Pipeline2D::Setup()
{
}

void Pine::Pipeline2D::Shutdown()
{
}

void Pine::Pipeline2D::Run(RenderingContext& context)
{
    static const auto renderItemSort = [](const RenderItem& a, const RenderItem& b)
    {
        return a.m_Order < b.m_Order;
    };

    // This should keep the capacity the same from the last frame, which should avoid
    // us having to do re-allocations all the time.
    m_RenderItems.clear();

    for (auto& spriteRenderer : Components::Get<SpriteRenderer>())
    {
        m_RenderItems.push_back({RenderItemType::SpriteRenderer, &spriteRenderer, spriteRenderer.GetOrder()});
    }

    for (auto& tilemapRenderer : Components::Get<TilemapRenderer>())
    {
        m_RenderItems.push_back({RenderItemType::TilemapRenderer, &tilemapRenderer, tilemapRenderer.GetOrder()});
    }

    std::sort(m_RenderItems.begin(), m_RenderItems.end(), renderItemSort);

    Renderer2D::PrepareFrame();

    for (const auto& renderItem : m_RenderItems)
    {
        const auto transform = renderItem.m_ComponentPointer->GetParent()->GetTransform();

        if (renderItem.m_Type == RenderItemType::SpriteRenderer)
        {
            auto spriteRenderer = dynamic_cast<SpriteRenderer*>(renderItem.m_ComponentPointer);

            auto uvScaling = Vector2f(1.f);

            if (spriteRenderer->GetScalingMode() == SpriteScalingMode::Repeat)
            {
                uvScaling *= Vector2f(transform->GetScale());
            }

            Vector2f size = Vector2f(spriteRenderer->GetTexture()->GetWidth(), spriteRenderer->GetTexture()->GetHeight()) * Vector2f(transform->GetScale());

            Renderer2D::AddFilledTexturedRectangle(transform->GetPosition(),
                                                   size,
                                                   Color(255, 255, 255, 255),
                                                   spriteRenderer->GetTexture(),
                                                   Vector2f(0.f),
                                                   uvScaling);
        }

        if (renderItem.m_Type == RenderItemType::TilemapRenderer)
        {
            auto tilemapRenderer = dynamic_cast<TilemapRenderer*>(renderItem.m_ComponentPointer);

            auto tileMap = tilemapRenderer->GetTilemap();
            auto textureAtlas = tileMap->GetTileset()->GetTextureAtlas();
            auto tileSize = static_cast<float>(tileMap->GetTileset()->GetTileSize());

            auto positionOffset = Vector2f(transform->GetPosition());

            for (auto& tile : tileMap->GetTiles())
            {
                Pine::Renderer2D::AddTextureAtlasItem(positionOffset + Pine::Vector2f(tileSize * static_cast<float>(tile.m_Position.x), tileSize * static_cast<float>(tile.m_Position.y)),
                                                      textureAtlas,
                                                      tile.m_RenderIndex,
                                                      Pine::Color(255, 255, 255, 255));
            }
        }
    }

    Renderer2D::RenderFrame(&context);
}
