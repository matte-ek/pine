#pragma once
#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"

namespace Pine
{

    struct RenderingContext
    {
        Vector2f m_Size = Vector2f(0.f);

        Camera* m_Camera = nullptr;

        int m_DrawCalls = 0;
    };

}