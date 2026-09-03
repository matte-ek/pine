#include "PostProcessing.hpp"

#include <chrono>

#include <Pine/Assets/Assets.hpp>
#include <Pine/Assets/Shader/Shader.hpp>
#include <Pine/Graphics/Graphics.hpp>

#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Features/Bloom/Bloom.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Assets/Level/Level.hpp"

namespace
{
    Pine::Shader* m_PostProcessingShader = nullptr;
    Pine::Graphics::IUniformVariable* m_PostProcessingViewportScale = nullptr;
    Pine::Graphics::IUniformVariable* m_PostProcessingTime = nullptr;
    Pine::Graphics::IUniformVariable* m_PostProcessingGrainStrength = nullptr;
    Pine::Graphics::IUniformVariable* m_PostProcessingVignetteStrength = nullptr;
    Pine::Graphics::IUniformVariable* m_PostProcessingExposure = nullptr;
    Pine::Graphics::IUniformVariable* m_PostProcessingBloomIntensity = nullptr;
}

void Pine::Rendering::PostProcessing::Setup()
{
    m_PostProcessingShader = Pine::Assets::Get<Shader>("engine/shaders/post-processing/post-process");
}

void Pine::Rendering::PostProcessing::Shutdown()
{
}

void Pine::Rendering::PostProcessing::Render(const RenderingContext *renderingContext, Graphics::IFrameBuffer *sceneFrameBuffer)
{
    PINE_PF_SCOPE();

    // Make sure Setup() has been called, and the engine files are available
    assert(m_PostProcessingShader != nullptr);

    // Setup target frame buffer
    if (renderingContext->FrameBuffer)
    {
        renderingContext->FrameBuffer->Bind();
    }
    else
    {
        Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr); // This will just render everything onto the screen.
    }

    // Prepare for rendering
    Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), renderingContext->Size);

    Graphics::GetGraphicsAPI()->ClearColor(Color(static_cast<int>(renderingContext->ClearColor.r * 255.f),
                                                 static_cast<int>(renderingContext->ClearColor.g * 255.f),
                                                 static_cast<int>(renderingContext->ClearColor.b * 255.f),
                                                 static_cast<int>(renderingContext->ClearColor.a * 255.f)));

    Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer | Graphics::DepthBuffer | Graphics::StencilBuffer);

    // We don't want any depth testing here as we're just rendering a 2D plane
    Graphics::GetGraphicsAPI()->SetDepthTestEnabled(false);

    m_PostProcessingShader->GetProgram()->Use();

    // Grab the viewport scale if we haven't
    if (m_PostProcessingViewportScale == nullptr)
    {
        m_PostProcessingViewportScale = m_PostProcessingShader->GetProgram()->GetUniformVariable("viewportScale");
    }

    // Shader is probably wrong for whatever reason
    assert(m_PostProcessingViewportScale != nullptr);

    // Grab the film-grain / vignette uniforms if we haven't (guarded - may be absent in older shaders).
    if (m_PostProcessingTime == nullptr)
    {
        m_PostProcessingTime = m_PostProcessingShader->GetProgram()->GetUniformVariable("time");
    }
    if (m_PostProcessingGrainStrength == nullptr)
    {
        m_PostProcessingGrainStrength = m_PostProcessingShader->GetProgram()->GetUniformVariable("grainStrength");
    }
    if (m_PostProcessingVignetteStrength == nullptr)
    {
        m_PostProcessingVignetteStrength = m_PostProcessingShader->GetProgram()->GetUniformVariable("vignetteStrength");
    }
    if (m_PostProcessingExposure == nullptr)
    {
        m_PostProcessingExposure = m_PostProcessingShader->GetProgram()->GetUniformVariable("exposure");
    }
    if (m_PostProcessingBloomIntensity == nullptr)
    {
        m_PostProcessingBloomIntensity = m_PostProcessingShader->GetProgram()->GetUniformVariable("bloomIntensity");
    }

    sceneFrameBuffer->GetColorBuffer()->Bind(0);
    AmbientOcclusion::GetOutputTexture()->Bind(1);
    Bloom::GetOutputTexture()->Bind(2);

    m_PostProcessingViewportScale->LoadVector2(Vector2f(renderingContext->Size.x / static_cast<float>(sceneFrameBuffer->GetSize().x),
                                                        renderingContext->Size.y / static_cast<float>(sceneFrameBuffer->GetSize().y)));

    if (m_PostProcessingTime != nullptr)
    {
        static const auto startTime = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - startTime).count();

        m_PostProcessingTime->LoadFloat(elapsed);
    }

    // Film look is authored per-level; fall back to sensible defaults if there's no active level.
    float grainStrength = 0.08f;
    float vignetteStrength = 0.5f;
    float exposure = 1.0f;
    float bloomIntensity = 0.6f;

    if (const auto level = World::GetActiveLevel())
    {
        grainStrength = level->GetLevelSettings().GrainStrength;
        vignetteStrength = level->GetLevelSettings().VignetteStrength;
        exposure = level->GetLevelSettings().Exposure;
        bloomIntensity = level->GetLevelSettings().BloomIntensity;
    }

    if (m_PostProcessingGrainStrength != nullptr)
    {
        m_PostProcessingGrainStrength->LoadFloat(grainStrength);
    }

    if (m_PostProcessingVignetteStrength != nullptr)
    {
        m_PostProcessingVignetteStrength->LoadFloat(vignetteStrength);
    }

    if (m_PostProcessingExposure != nullptr)
    {
        m_PostProcessingExposure->LoadFloat(exposure);
    }

    if (m_PostProcessingBloomIntensity != nullptr)
    {
        m_PostProcessingBloomIntensity->LoadFloat(bloomIntensity);
    }

    Common::QuadTarget::Render();
}