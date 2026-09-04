#pragma once
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"

namespace Pine
{

    struct RenderingStatistics
    {
        int LightCount = 0;
        int ModelLightCalculationCount = 0;
        int DrawCalls = 0;
        std::uint64_t VertexCount = 0;
        double RenderTime = 0.f;

        int VisibleObjectCount = 0;
        int CulledObjectCount = 0;

        void Reset()
        {
            LightCount = 0;
            ModelLightCalculationCount = 0;
            DrawCalls = 0;
            VertexCount = 0;
            RenderTime = 0.f;
            VisibleObjectCount = 0;
            CulledObjectCount = 0;
        }
    };

    struct RenderingContext
    {
        bool Active = true;
        bool UseRenderPipeline = true;

        Vector2f Size = Vector2f(0.f);

        Camera* SceneCamera = nullptr;

        Graphics::IFrameBuffer* FrameBuffer = nullptr;

        Vector4f ClearColor = Vector4f(0.f, 0.f, 0.f, 1.f);

        Texture3D* Skybox = nullptr;

        bool EnableStencilBuffer = true;

        RenderingStatistics Statistics;

        // What this context's camera can see. Owned per context because visibility depends on the
        // frustum: two viewports looking different ways cull differently, and both stages of this
        // context's frame read the same set.
        Rendering::RenderCulling::VisibilitySet Visibility;

        int PreAllocItems = 0;
    };

}