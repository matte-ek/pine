#pragma once
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"

namespace Pine
{

    struct RenderingContext
    {
        bool m_Active = true;

        Vector2f m_Size = Vector2f(0.f);

        Camera* m_Camera = nullptr;
        Graphics::IFrameBuffer* m_FrameBuffer = nullptr;

        Vector4f m_ClearColor = Vector4f(0.f, 0.f, 0.f, 1.f);

        int m_DrawCalls = 0;
    };

}