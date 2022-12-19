#pragma once

#include <string>

#include "Pine/Core/Color/Color.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Pine::Renderer2D
{

    void UseRenderingContext();

    void PrepareFrame();
    void RenderFrame();

    void AddRectangle(Vector2f position, Vector2f size, Color color);
    void AddFilledRectangle(Vector2f position, Vector2f size, Color color);
    void AddFilledRoundedRectangle(Vector2f position, Vector2f size, float radius, Color color);

    void AddText(Vector2f position, Color color, const std::string& str);


}