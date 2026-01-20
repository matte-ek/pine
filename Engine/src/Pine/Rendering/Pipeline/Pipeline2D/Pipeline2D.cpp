#include "Pipeline2D.hpp"

#include <GL/glew.h>

#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
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

    struct Drawable
    {
        RenderItemType m_Type = RenderItemType::SpriteRenderer;
        Pine::IComponent* m_ComponentPointer = nullptr;
        int m_Order = 0;
    };

    std::vector<Drawable> m_RenderItems;
}

void Pine::Pipeline2D::Setup()
{
}

void Pine::Pipeline2D::Shutdown()
{
}

void Pine::Pipeline2D::Run(RenderingContext& context)
{
    PINE_PF_SCOPE();

    static const auto renderItemSort = [](const Drawable& a, const Drawable& b)
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
    Renderer2D::SetCoordinateSystem(Rendering::CoordinateSystem::World);

    for (const auto& renderItem : m_RenderItems)
    {
        const auto transform = renderItem.m_ComponentPointer->GetParent()->GetTransform();

        if (renderItem.m_Type == RenderItemType::SpriteRenderer)
        {
            auto spriteRenderer = dynamic_cast<SpriteRenderer*>(renderItem.m_ComponentPointer);

            if (spriteRenderer->GetTexture() == nullptr)
            {
                continue;
            }

            auto uvScaling = Vector2f(1.f);

            if (spriteRenderer->GetScalingMode() == SpriteScalingMode::Repeat)
            {
                uvScaling *= Vector2f(transform->GetScale());
            }

            Vector2f size = Vector2f(spriteRenderer->GetTexture()->GetWidth(), spriteRenderer->GetTexture()->GetHeight()) * Vector2f(transform->GetScale());
            Color color = Color(spriteRenderer->GetColor().x * 255.f, spriteRenderer->GetColor().y * 255.f, spriteRenderer->GetColor().z * 255.f, spriteRenderer->GetColor().w * 255.f);

            Renderer2D::AddFilledTexturedRectangle(transform->GetPosition(),
                                                   size,
                                                   0.f,
                                                   color,
                                                   spriteRenderer->GetTexture(),
                                                   Vector2f(0.f),
                                                   uvScaling);
        }

        if (renderItem.m_Type == RenderItemType::TilemapRenderer)
        {
            auto tilemapRenderer = dynamic_cast<TilemapRenderer*>(renderItem.m_ComponentPointer);

            if (tilemapRenderer->GetTilemap() == nullptr ||
                tilemapRenderer->GetTilemap()->GetTileset() == nullptr)
            {
                continue;
            }

            auto tileMap = tilemapRenderer->GetTilemap();
            auto textureAtlas = tileMap->GetTileset()->GetTextureAtlas();
            auto tileSize = static_cast<float>(tileMap->GetTileset()->GetTileSize()) * transform->GetScale().x;

            auto positionOffset = Vector2f(transform->GetPosition()) * context.Size;

            Renderer2D::SetCoordinateSystem(Rendering::CoordinateSystem::Screen);

            for (auto& tile : tileMap->GetTiles())
            {
                if (tile.m_Flags & TileFlags_Hidden)
                    continue;

                Renderer2D::AddTextureAtlasItem(positionOffset + Vector2f(tileSize * static_cast<float>(tile.m_Position.x), tileSize * static_cast<float>(tile.m_Position.y)),
                                                tileSize,
                                                textureAtlas,
                                                tile.m_RenderIndex,
                                                Color(255, 255, 255, 255));
            }

            Renderer2D::SetCoordinateSystem(Rendering::CoordinateSystem::World);
        }
    }

    auto oldCoordinateSystem = Renderer2D::GetCoordinateSystem();

    Pine::Graphics::GetGraphicsAPI()->SetDepthTestEnabled(false);

    Pine::Graphics::GetGraphicsAPI()->SetBlendingEnabled(true);
    Pine::Graphics::GetGraphicsAPI()->SetBlendingFunction(Pine::Graphics::BlendingFunction::SourceAlpha, Pine::Graphics::BlendingFunction::OneMinusSourceAlpha);

    Renderer2D::RenderFrame(&context);

    Pine::Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);

    Renderer2D::SetCoordinateSystem(oldCoordinateSystem);
}
