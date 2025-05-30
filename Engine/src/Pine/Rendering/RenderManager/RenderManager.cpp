#include "RenderManager.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Rendering/Pipeline/Pipeline2D/Pipeline2D.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include <vector>
#include <GLFW/glfw3.h>

#include "Pine/Core/Timer/Timer.hpp"
#include "Pine/Rendering/Features/PostProcessing/PostProcessing.hpp"

namespace
{
    // All rendering contexts being used
    std::vector<Pine::RenderingContext*> m_RenderingContexts;

    // The rendering context that is used to render the scene
    Pine::RenderingContext *m_CurrentRenderingContext;

    // A fallback "default" rendering context to quickly get up and running.
    Pine::RenderingContext m_DefaultRenderingContext;

    // The frame buffer used when rendering internally
    Pine::Graphics::IFrameBuffer* m_InternalFrameBuffer;

    // Used to track delta time between frames
    double m_LastFrameTime = 0;

    std::vector<std::function<void(Pine::RenderingContext*, Pine::RenderStage, float)>> m_RenderCallbackFunctions;

    void CallRenderCallback(Pine::RenderingContext* context, Pine::RenderStage stage, float deltaTime)
    {
        for (const auto &func: m_RenderCallbackFunctions)
        {
            func(context, stage, deltaTime);
        }
    }
}

void Pine::RenderManager::Setup()
{
    m_InternalFrameBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_InternalFrameBuffer->Create(1920, 1080, Graphics::Buffers::ColorBuffer | Graphics::Buffers::DepthBuffer | Graphics::Buffers::StencilBuffer);

    m_DefaultRenderingContext.Size = Vector2f(Engine::GetEngineConfiguration().m_WindowSize);
    
    SetPrimaryRenderingContext(&m_DefaultRenderingContext);

    Pipeline2D::Setup();
    Pipeline3D::Setup();
    Rendering::PostProcessing::Setup();
}

void Pine::RenderManager::Shutdown()
{
    Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_InternalFrameBuffer);

    Rendering::PostProcessing::Shutdown();
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

    static auto engineConfig = Engine::GetEngineConfiguration();

    double currentFrameTime = glfwGetTime();

    double deltaTime = currentFrameTime - m_LastFrameTime;
    auto fDeltaTime = static_cast<float>(deltaTime);

    m_LastFrameTime = currentFrameTime;

    CallRenderCallback(nullptr, RenderStage::PreRender, fDeltaTime);

    // If we're in for example the editor, we'll always want to update
    // the transformation matrices etc.
    if (!engineConfig.m_ProductionMode)
    {
        for (auto& transform : Components::Get<Transform>(true))
        {
            transform.OnRender(fDeltaTime);
        }
    }

    for (const auto renderingContext : m_RenderingContexts)
    {
        if (World::GetActiveLevel())
        {
            renderingContext->Skybox = World::GetActiveLevel()->GetLevelSettings().Skybox.Get();
        }

        if (!renderingContext->Active)
        {
            continue;
        }

        m_CurrentRenderingContext = renderingContext;

        // Reset statistics
        renderingContext->DrawCalls = 0;
        renderingContext->VertexCount = 0;
        renderingContext->RenderTime = 0;

        Timer renderTime;

        m_InternalFrameBuffer->Bind();

        // If we're not running in the editor, only update the scene camera.
        if (engineConfig.m_ProductionMode)
        {
            // Make sure we got the camera's projection and view matrix ready for the scene
            if (renderingContext->SceneCamera)
            {
                renderingContext->SceneCamera->GetParent()->GetTransform()->OnRender(0.f);
                renderingContext->SceneCamera->OnRender(0.f);
            }
        }
        else
        {
            for (auto& camera : Components::Get<Camera>(true))
            {
                camera.OnRender(fDeltaTime);
            }
        }

        Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), renderingContext->Size);

        Graphics::GetGraphicsAPI()->ClearColor(Color(static_cast<int>(renderingContext->ClearColor.r * 255.f),
                                                     static_cast<int>(renderingContext->ClearColor.g * 255.f),
                                                     static_cast<int>(renderingContext->ClearColor.b * 255.f),
                                                     static_cast<int>(renderingContext->ClearColor.a * 255.f)));

        Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer | Graphics::DepthBuffer | Graphics::StencilBuffer);

        Graphics::GetGraphicsAPI()->SetStencilTestEnabled(renderingContext->EnableStencilBuffer);
        
        if (renderingContext->EnableStencilBuffer)
        {
            Graphics::GetGraphicsAPI()->SetStencilFunction(Graphics::TestFunction::Always, 0, 0);
            Graphics::GetGraphicsAPI()->SetStencilOperation(Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep);
        }

        CallRenderCallback(renderingContext, RenderStage::RenderContext, fDeltaTime);

        if (renderingContext->UseRenderPipeline)
        {
            // 3D pass
            CallRenderCallback(renderingContext, RenderStage::PreRender3D, fDeltaTime);
            Pipeline3D::Run(*renderingContext);
            CallRenderCallback(renderingContext, RenderStage::PostRender3D, fDeltaTime);

            // 2D pass
            CallRenderCallback(renderingContext, RenderStage::PreRender2D, fDeltaTime);
            Pipeline2D::Run(*renderingContext);
            CallRenderCallback(renderingContext, RenderStage::PostRender2D, fDeltaTime);

            // Post Processing
            CallRenderCallback(renderingContext, RenderStage::PostProcessing, fDeltaTime);

            Rendering::PostProcessing::Render(renderingContext, m_InternalFrameBuffer);
        }

        renderTime.Stop();

        renderingContext->RenderTime = renderTime.GetElapsedTime();
    }

    Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr);

    CallRenderCallback(nullptr, RenderStage::PostRender, fDeltaTime);
}

void Pine::RenderManager::AddRenderCallback(const std::function<void(RenderingContext*, RenderStage, float)> &func)
{
    m_RenderCallbackFunctions.push_back(func);
}

void Pine::RenderManager::SetPrimaryRenderingContext(RenderingContext* context)
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

void Pine::RenderManager::AddRenderingContextPass(RenderingContext* context)
{
    m_RenderingContexts.push_back(context);
}

void Pine::RenderManager::RemoveRenderingContextPass(const RenderingContext* context)
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