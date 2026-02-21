#include "PostProcessing.hpp"

#include <Pine/Assets/Assets.hpp>
#include <Pine/Assets/Shader/Shader.hpp>
#include <Pine/Graphics/Graphics.hpp>

#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"

namespace
{
    Pine::Shader* m_PostProcessingShader = nullptr;
    Pine::Graphics::IUniformVariable* m_PostProcessingViewportScale = nullptr;
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

    sceneFrameBuffer->GetColorBuffer()->Bind(0);
    AmbientOcclusion::GetOutputTexture()->Bind(1);

    m_PostProcessingViewportScale->LoadVector2(Vector2f(renderingContext->Size.x / static_cast<float>(sceneFrameBuffer->GetSize().x),
                                                        renderingContext->Size.y / static_cast<float>(sceneFrameBuffer->GetSize().y)));

    Common::QuadTarget::Render();
}