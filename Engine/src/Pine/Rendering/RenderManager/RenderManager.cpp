#include "RenderManager.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Rendering/Pipeline/Pipeline2D/Pipeline2D.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Script/ScriptManager.hpp"
#include <algorithm>
#include <vector>
#include <GLFW/glfw3.h>

#include "Pine/Core/Timer/Timer.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Common/Blur/Blur.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"
#include "Pine/Rendering/Features/PostProcessing/PostProcessing.hpp"
#include "Pine/Rendering/Features/Bloom/Bloom.hpp"
#include "Pine/Rendering/InternalResolution/InternalResolution.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"

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
    double m_DeltaTime = 0;

    std::vector<std::function<void(Pine::RenderingContext*, Pine::RenderStage, float)>> m_RenderCallbackFunctions;

    void CallRenderCallback(Pine::RenderingContext* context, const Pine::RenderStage stage, const float deltaTime)
    {
        for (const auto &func: m_RenderCallbackFunctions)
        {
            func(context, stage, deltaTime);
        }
    }

    void CreateInternalFrameBuffer()
    {
        m_InternalFrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
        m_InternalFrameBuffer->Prepare();

        // The scene renders into this buffer in HDR: a float (RGBA16F) color target lets lighting
        // accumulate values above 1.0 without clipping. Tone mapping + gamma in the post-process
        // resolve pass bring it back down to the 8-bit output target for display.
        const auto resolution = Pine::Rendering::InternalResolution::Get();

        m_InternalFrameBuffer->AttachTextures(
            resolution.x,
            resolution.y,
            Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::Buffers::DepthBuffer | Pine::Graphics::Buffers::StencilBuffer,
            0,
            Pine::Graphics::TextureFormat::RGBA16F);

        m_InternalFrameBuffer->Finish();
    }

    // The largest context that will be drawn this frame, which is what the shared scene buffers
    // have to be able to hold.
    Pine::Vector2i LargestActiveContextSize()
    {
        Pine::Vector2i largest(0);

        for (const auto context : m_RenderingContexts)
        {
            if (context == nullptr || !context->Active)
            {
                continue;
            }

            largest.x = std::max(largest.x, static_cast<int>(context->Size.x));
            largest.y = std::max(largest.y, static_cast<int>(context->Size.y));
        }

        return largest;
    }
}

void Pine::RenderManager::Setup()
{
    CreateInternalFrameBuffer();

    Rendering::InternalResolution::AddResizeCallback([]
    {
        Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_InternalFrameBuffer);

        CreateInternalFrameBuffer();
    });

    m_DefaultRenderingContext.Size = Vector2f(Engine::GetEngineConfiguration().m_WindowSize);
    
    SetPrimaryRenderingContext(&m_DefaultRenderingContext);

    Pipeline2D::Setup();
    Pipeline3D::Setup();

    Rendering::Common::QuadTarget::Setup();
    Rendering::Common::Blur::Setup();
    Rendering::PostProcessing::Setup();
    Rendering::Bloom::Setup();
}

void Pine::RenderManager::Shutdown()
{
    Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_InternalFrameBuffer);

    Rendering::Common::QuadTarget::Shutdown();
    Rendering::Common::Blur::Shutdown();
    Rendering::Bloom::Shutdown();
    Rendering::PostProcessing::Shutdown();
    Pipeline3D::Shutdown();
    Pipeline2D::Shutdown();
}

void Pine::RenderManager::Run()
{
    // Named by hand so that the editor's profiler can look the scope up without matching against a
    // function signature that any refactor would change.
    PINE_PF_SCOPE_MANUAL("Pine::RenderManager::Run");

    // Make sure we have at least one rendering context ready.
    if (m_RenderingContexts.empty() || m_RenderingContexts[0] == nullptr)
    {
        return;
    }

    static auto engineConfig = Engine::GetEngineConfiguration();

    // Before anything binds a scene buffer: a context that grew past the current allocation (a
    // resized window, a dragged viewport splitter) needs them rebuilt to fit first.
    Rendering::InternalResolution::Internal::GrowTo(LargestActiveContextSize());

    double currentFrameTime = glfwGetTime();

    double deltaTime = currentFrameTime - m_LastFrameTime;
    auto fDeltaTime = static_cast<float>(deltaTime);

    m_DeltaTime = deltaTime;
    m_LastFrameTime = currentFrameTime;

    CallRenderCallback(nullptr, RenderStage::PreRender, fDeltaTime);

    // Once per frame, after physics and every OnUpdate, and before any transform or camera below
    // is read for drawing - so a script can make last-moment changes, such as a camera following a
    // body physics just moved, and have this frame show them. Gated exactly like OnUpdate, so
    // scripts never run in the editor outside play mode, nor when nothing updates the world.
    if (!engineConfig.m_Standalone && !World::IsPaused())
    {
        Script::Manager::OnRender(fDeltaTime);
    }

    // If we're in for example the editor, we'll always want to update
    // the transformation matrices etc.
    if (!engineConfig.m_ProductionMode)
    {
        for (auto& transform : Components::Get<Transform>(true))
        {
            transform.OnRender(fDeltaTime);
        }
    }

    Pipeline3D::Prepare();

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

        // Reset statistics. Done before the pre-pass so the shadow, depth and AO draw calls it
        // issues are counted against this context rather than wiped afterwards.
        renderingContext->Statistics.Reset();

        Timer renderTime;

        // The camera has to be up to date *before* the pre-pass, not after it: the pre-pass renders
        // the depth buffer that ambient occlusion consumes, so updating the matrices later meant
        // both ran on the previous frame's viewpoint and the AO lagged behind the geometry
        // whenever the camera moved.
        //
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

        if (renderingContext->UseRenderPipeline)
        {
            Pipeline3D::Run(*renderingContext, PipelineStage::Prepass);
        }

        m_InternalFrameBuffer->Bind();

        Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), renderingContext->Size);

        // This clear fills the linear HDR scene buffer, so the authored sRGB colour is decoded
        // first - otherwise the post-process resolve encodes a background that was never decoded
        // and it comes out too light. (Color is 8-bit, so very dark backgrounds band a little; that
        // is inherited from the ClearColor API rather than introduced here.)
        const auto linearClearColor = SrgbToLinear(renderingContext->ClearColor);

        Graphics::GetGraphicsAPI()->ClearColor(Color(static_cast<int>(linearClearColor.r * 255.f),
                                                     static_cast<int>(linearClearColor.g * 255.f),
                                                     static_cast<int>(linearClearColor.b * 255.f),
                                                     static_cast<int>(linearClearColor.a * 255.f)));

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
            Pipeline3D::Run(*renderingContext, PipelineStage::Default);
            CallRenderCallback(renderingContext, RenderStage::PostRender3D, fDeltaTime);

            // 2D pass
            CallRenderCallback(renderingContext, RenderStage::PreRender2D, fDeltaTime);
            Pipeline2D::Run(*renderingContext);
            CallRenderCallback(renderingContext, RenderStage::PostRender2D, fDeltaTime);

            // Post Processing
            CallRenderCallback(renderingContext, RenderStage::PostProcessing, fDeltaTime);

            // Bloom reads the finished HDR scene and produces the glow the resolve pass composites.
            // When disabled its output buffer is blacked out, so the composite becomes a no-op -
            // the same trick ambient occlusion uses (see Pipeline3D).
            if (Pipeline3D::GetPipelineConfiguration().RenderBloom)
            {
                Rendering::Bloom::Run(*renderingContext, m_InternalFrameBuffer);
            }
            else
            {
                Rendering::Bloom::ClearOutput();
            }

            Rendering::PostProcessing::Render(renderingContext, m_InternalFrameBuffer);
        }

        renderTime.Stop();

        renderingContext->Statistics.RenderTime = renderTime.GetElapsedTime();
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

const std::vector<Pine::RenderingContext*>& Pine::RenderManager::GetRenderingContexts()
{
    return m_RenderingContexts;
}

Pine::RenderingContext *Pine::RenderManager::GetDefaultRenderingContext()
{
    return &m_DefaultRenderingContext;
}

Pine::Graphics::IFrameBuffer * Pine::RenderManager::GetInternalFrameBuffer()
{
    return m_InternalFrameBuffer;
}

double Pine::RenderManager::GetGlobalDeltaTime()
{
    return m_DeltaTime;
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