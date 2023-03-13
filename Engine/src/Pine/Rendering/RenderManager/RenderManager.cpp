#include "RenderManager.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Rendering/Pipeline/Pipeline2D/Pipeline2D.hpp"
#include <vector>

namespace
{
    // All rendering contexts being used
    std::vector<Pine::RenderingContext*> m_RenderingContexts;

    // The rendering context that is used to render the scene
    Pine::RenderingContext* m_CurrentRenderingContext;

    // A fallback "default" rendering context to quickly get up and running.
    Pine::RenderingContext m_DefaultRenderingContext;

    std::vector<std::function<void(Pine::RenderStage)>> m_RenderCallbackFunctions;

    void CallRenderCallback(Pine::RenderStage stage)
    {
        for (const auto& func : m_RenderCallbackFunctions)
        {
            func(stage);
        }
    }

}

void Pine::RenderManager::Setup()
{
    m_DefaultRenderingContext.m_Size = Vector2f(Engine::GetEngineConfiguration().m_WindowSize);

    SetPrimaryRenderingContext(&m_DefaultRenderingContext);

    Pipeline2D::Setup();
}

void Pine::RenderManager::Shutdown()
{
    Pipeline2D::Shutdown();
}

void Pine::RenderManager::Run()
{
    // Make sure we have at least one rendering context ready.
    if (m_RenderingContexts.empty() || m_RenderingContexts[0] == nullptr)
    {
        return;
    }

    CallRenderCallback(RenderStage::PreRender);

    for (auto renderingContext : m_RenderingContexts)
    {
        if (!renderingContext->m_Active)
            continue;

        m_CurrentRenderingContext = renderingContext;

        // Reset statistics
        renderingContext->m_DrawCalls = 0;

        if (renderingContext->m_FrameBuffer)
            renderingContext->m_FrameBuffer->Bind();
        else
            Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr); // This will just render everything onto the screen.

        // Make sure we got the camera's projection and view matrix ready for the scene
        if (renderingContext->m_Camera)
            renderingContext->m_Camera->OnRender(0.f);

        Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), renderingContext->m_Size);

        Graphics::GetGraphicsAPI()->ClearColor(Color(static_cast<int>(renderingContext->m_ClearColor.r * 255.f),
                                                           static_cast<int>(renderingContext->m_ClearColor.g * 255.f),
                                                           static_cast<int>(renderingContext->m_ClearColor.b * 255.f),
                                                           static_cast<int>(renderingContext->m_ClearColor.a * 255.f)));

        Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer | Graphics::DepthBuffer);

        CallRenderCallback(RenderStage::Render2D);

        Pipeline2D::Run(*renderingContext);

        CallRenderCallback(RenderStage::Render3D);
    }

    Pine::Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr);

    CallRenderCallback(RenderStage::PostRender);
}

void Pine::RenderManager::AddRenderCallback(const std::function<void(RenderStage)>& func)
{
    m_RenderCallbackFunctions.push_back(func);
}

void Pine::RenderManager::SetPrimaryRenderingContext(Pine::RenderingContext* context)
{
    if (m_RenderingContexts.empty())
        m_RenderingContexts.push_back(&m_DefaultRenderingContext);

    m_RenderingContexts[0] = context;
}

Pine::RenderingContext* Pine::RenderManager::GetPrimaryRenderingContext()
{
    return m_RenderingContexts[0];
}

Pine::RenderingContext* Pine::RenderManager::GetCurrentRenderingContext()
{
    return m_CurrentRenderingContext;
}

Pine::RenderingContext* Pine::RenderManager::GetDefaultRenderingContext()
{
    return &m_DefaultRenderingContext;
}

void Pine::RenderManager::AddRenderingContextPass(Pine::RenderingContext* context)
{
    m_RenderingContexts.push_back(context);
}

void Pine::RenderManager::RemoveRenderingContextPass(Pine::RenderingContext* context)
{
    for (int i = 0; i < m_RenderingContexts.size();i++)
    {
        if (m_RenderingContexts[i] == context)
        {
            m_RenderingContexts.erase(m_RenderingContexts.begin() + i);
            break;
        }
    }
}