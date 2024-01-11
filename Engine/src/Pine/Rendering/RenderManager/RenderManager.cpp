#include "RenderManager.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Rendering/Pipeline/Pipeline2D/Pipeline2D.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include <vector>
#include <GLFW/glfw3.h>

namespace
{
    // All rendering contexts being used
    std::vector<Pine::RenderingContext *> m_RenderingContexts;

    // The rendering context that is used to render the scene
    Pine::RenderingContext *m_CurrentRenderingContext;

    // A fallback "default" rendering context to quickly get up and running.
    Pine::RenderingContext m_DefaultRenderingContext;

    // Used to track delta time between frames
    double m_LastFrameTime = 0;

    std::vector<std::function<void(Pine::RenderStage, float)>> m_RenderCallbackFunctions;

    void CallRenderCallback(Pine::RenderStage stage, float deltaTime)
    {
        for (const auto &func: m_RenderCallbackFunctions)
        {
            func(stage, deltaTime);
        }
    }

}

void Pine::RenderManager::Setup()
{
    m_DefaultRenderingContext.Size = Vector2f(Engine::GetEngineConfiguration().m_WindowSize);
    
    SetPrimaryRenderingContext(&m_DefaultRenderingContext);

    Pipeline2D::Setup();
    Pipeline3D::Setup();
}

void Pine::RenderManager::Shutdown()
{
    Pipeline3D::Shutdown();
    Pipeline2D::Shutdown();
}

void Pine::RenderManager::Run()
{
    // Make sure we have at least one rendering context ready.
    if (m_RenderingContexts.empty() || m_RenderingContexts[0] == nullptr)
    {
        return;
    }

    double currentFrameTime = glfwGetTime();

    double deltaTime = currentFrameTime - m_LastFrameTime;
    auto fDeltaTime = static_cast<float>(deltaTime);

    m_LastFrameTime = currentFrameTime;

    CallRenderCallback(RenderStage::PreRender, fDeltaTime);

    for (auto renderingContext: m_RenderingContexts)
    {
        if (!renderingContext->Active)
            continue;

        m_CurrentRenderingContext = renderingContext;

        // Reset statistics
        renderingContext->DrawCalls = 0;

        if (renderingContext->FrameBuffer)
            renderingContext->FrameBuffer->Bind();
        else
            Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr); // This will just render everything onto the screen.

        // Make sure we got the camera's projection and view matrix ready for the scene
        if (renderingContext->SceneCamera)
            renderingContext->SceneCamera->OnRender(0.f);

        Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), renderingContext->Size);

        Graphics::GetGraphicsAPI()->ClearColor(Color(static_cast<int>(renderingContext->ClearColor.r * 255.f),
                                                     static_cast<int>(renderingContext->ClearColor.g * 255.f),
                                                     static_cast<int>(renderingContext->ClearColor.b * 255.f),
                                                     static_cast<int>(renderingContext->ClearColor.a * 255.f)));

        Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer | Graphics::DepthBuffer);

        // 3D pass
        CallRenderCallback(RenderStage::Render3D, fDeltaTime);
        Pipeline3D::Run(*renderingContext);

        // 2D pass
        CallRenderCallback(RenderStage::Render2D, fDeltaTime);
        Pipeline2D::Run(*renderingContext);

        // Post Processing
        CallRenderCallback(RenderStage::PostProcessing, fDeltaTime);
    }

    Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr);

    CallRenderCallback(RenderStage::PostRender, fDeltaTime);
}

void Pine::RenderManager::AddRenderCallback(const std::function<void(RenderStage, float)> &func)
{
    m_RenderCallbackFunctions.push_back(func);
}

void Pine::RenderManager::SetPrimaryRenderingContext(RenderingContext*context)
{
    if (m_RenderingContexts.empty())
        m_RenderingContexts.push_back(&m_DefaultRenderingContext);

    m_RenderingContexts[0] = context;
}

Pine::RenderingContext *Pine::RenderManager::GetPrimaryRenderingContext()
{
    return m_RenderingContexts[0];
}

Pine::RenderingContext *Pine::RenderManager::GetCurrentRenderingContext()
{
    return m_CurrentRenderingContext;
}

Pine::RenderingContext *Pine::RenderManager::GetDefaultRenderingContext()
{
    return &m_DefaultRenderingContext;
}

void Pine::RenderManager::AddRenderingContextPass(RenderingContext*context)
{
    m_RenderingContexts.push_back(context);
}

void Pine::RenderManager::RemoveRenderingContextPass(RenderingContext*context)
{
    for (int i = 0; i < m_RenderingContexts.size(); i++)
    {
        if (m_RenderingContexts[i] == context)
        {
            m_RenderingContexts.erase(m_RenderingContexts.begin() + i);
            break;
        }
    }
}