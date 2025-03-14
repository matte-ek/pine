#include "PostProcessing.hpp"

#include <Pine/Assets/Assets.hpp>
#include <Pine/Assets/Shader/Shader.hpp>
#include <Pine/Graphics/Graphics.hpp>
#include <Pine/Graphics/Interfaces/IVertexArray.hpp>

namespace
{
    Pine::Graphics::IVertexArray* m_RectangleVertexArray = nullptr;

    Pine::Shader* m_PostProcessingShader = nullptr;
    Pine::Graphics::IUniformVariable* m_PostProcessingViewportScale = nullptr;
}

void Pine::Rendering::PostProcessing::Setup()
{
    const std::vector quads =
    {
        -1.f, 1.f, 0.f,
        -1.f, -1.f, 0.f,
        1.f, -1.f, 0.f,
        1.f, 1.f, 0.f,
    };

    const std::vector<std::uint32_t> indices =
    {
        0, 1, 3,
        3, 1, 2
    };

    const std::vector uvs =
    {
        0.f, 0.f,
        0.f, 1.f,
        1.f, 1.f,
        1.f, 0.f
    };

    m_RectangleVertexArray = Graphics::GetGraphicsAPI()->CreateVertexArray();
    m_RectangleVertexArray->Bind();

    m_RectangleVertexArray->StoreFloatArrayBuffer(const_cast<float*>(quads.data()), quads.size() * sizeof(float), 0, 3, Graphics::BufferUsageHint::StaticDraw);
    m_RectangleVertexArray->StoreFloatArrayBuffer(const_cast<float*>(uvs.data()), uvs.size() * sizeof(float), 1, 2, Graphics::BufferUsageHint::StaticDraw);
    m_RectangleVertexArray->StoreElementArrayBuffer(const_cast<std::uint32_t*>(indices.data()), indices.size() * sizeof(int));

    m_PostProcessingShader = Pine::Assets::Get<Shader>("engine/shaders/post-processing/post-process.shader");
}

void Pine::Rendering::PostProcessing::Shutdown()
{
    Graphics::GetGraphicsAPI()->DestroyVertexArray(m_RectangleVertexArray);
}

void Pine::Rendering::PostProcessing::Render(const RenderingContext *renderingContext, Graphics::IFrameBuffer *sceneFrameBuffer)
{
    // Make sure Setup() has been called, and the engine files are available
    assert(m_RectangleVertexArray != nullptr);
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

    m_RectangleVertexArray->Bind();
    m_PostProcessingShader->GetProgram()->Use();

    // Grab the viewport scale if we haven't
    if (m_PostProcessingViewportScale == nullptr)
    {
        m_PostProcessingViewportScale = m_PostProcessingShader->GetProgram()->GetUniformVariable("viewportScale");
    }

    // Shader is probably wrong for whatever reason
    assert(m_PostProcessingViewportScale != nullptr);

    sceneFrameBuffer->GetColorBuffer()->Bind();

    m_PostProcessingViewportScale->LoadVector2(Vector2f(renderingContext->Size.x / static_cast<float>(sceneFrameBuffer->GetSize().x),
                                                        renderingContext->Size.y / static_cast<float>(sceneFrameBuffer->GetSize().y)));

    Graphics::GetGraphicsAPI()->DrawElements(Graphics::RenderMode::Triangles, 12);
}