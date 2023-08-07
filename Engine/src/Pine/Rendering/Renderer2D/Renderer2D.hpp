#pragma once

#include <string>

#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/Color/Color.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Rendering/RenderingContext.hpp"
#include "Pine/Rendering/Rendering.hpp"

namespace Pine
{
}

namespace Pine::Graphics
{
    class TextureAtlas;
}

namespace Pine::Renderer2D
{

    void PrepareFrame();
    void RenderFrame(RenderingContext* context);

    void SetCoordinateSystem(Rendering::CoordinateSystem coordinateSystem);
    Rendering::CoordinateSystem GetCoordinateSystem();

    void AddFilledRectangle(Vector2f position, Vector2f size, float rotation, Color color);

    void AddFilledTexturedRectangle(Vector2f position, Vector2f size, float rotation, Color color, const Texture2D* texture, Vector2f uvOffset = Vector2f(0.f), Vector2f uvScale = Vector2f(1.f));
    void AddFilledTexturedRectangle(Vector2f position, Vector2f size, float rotation, Color color, Graphics::ITexture* texture, Vector2f uvOffset = Vector2f(0.f), Vector2f uvScale = Vector2f(1.f));

    void AddFilledRoundedRectangle(Vector2f position, Vector2f size, float radius, Color color);

    void AddTextureAtlasItem(Vector2f position, float size, const Graphics::TextureAtlas* atlas, std::uint32_t itemId, Color color);

    void AddText(Vector2f position, Color color, const std::string& str);


}