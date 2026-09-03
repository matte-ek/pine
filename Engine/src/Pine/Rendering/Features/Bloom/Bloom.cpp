#include "Bloom.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Common/Blur/Blur.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Assets/Level/Level.hpp"

using namespace Pine;

namespace
{
    // Bloom is a wide, soft glow, so it runs at reduced resolution (cheaper, and the blur is wider
    // relative to the image). More blur passes = a broader glow.
    constexpr int BLOOM_RES_DIVISOR = 2;
    constexpr int BLOOM_BLUR_PASSES = 6;

    Shader* m_BloomExtractShader = nullptr;
    Graphics::IFrameBuffer* m_ExtractBuffer = nullptr;
    Rendering::Common::Blur::BlurContext m_BlurContext;

    int BloomWidth() { return Renderer3D::Specifications::General::INTERNAL_WIDTH / BLOOM_RES_DIVISOR; }
    int BloomHeight() { return Renderer3D::Specifications::General::INTERNAL_HEIGHT / BLOOM_RES_DIVISOR; }

    void CreateExtractBuffer()
    {
        m_ExtractBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
        m_ExtractBuffer->Prepare();

        const auto tex = Graphics::GetGraphicsAPI()->CreateTexture();
        tex->Bind();
        // RGBA16F: the extracted brightness is still HDR (values well above 1.0) until it's composited.
        tex->UploadTextureData(BloomWidth(), BloomHeight(), 0, Graphics::TextureFormat::RGBA16F, Graphics::TextureDataFormat::Float, nullptr);
        tex->SetFilteringMode(Graphics::TextureFilteringMode::Linear);

        m_ExtractBuffer->AttachTexture(tex, Graphics::BufferAttachment::Color);
        m_ExtractBuffer->Finish();
    }
}

void Rendering::Bloom::Setup()
{
    m_BloomExtractShader = Assets::Get<Shader>("engine/shaders/post-processing/bloom-extract");

    assert(m_BloomExtractShader != nullptr);

    CreateExtractBuffer();

    m_BlurContext.UseHDR = true;
    m_BlurContext.PassCount = BLOOM_BLUR_PASSES;
    m_BlurContext.Width = BloomWidth();
    m_BlurContext.Height = BloomHeight();
    m_BlurContext.Create();

    m_BlurContext.TargetBuffer->GetColorBuffer()->SetFilteringMode(Graphics::TextureFilteringMode::Linear);
    m_BlurContext.IntermediateBuffer->GetColorBuffer()->SetFilteringMode(Graphics::TextureFilteringMode::Linear);

    // Clear the output so that before the first run (or a scene with nothing bright) bloom adds nothing.
    m_BlurContext.TargetBuffer->Bind();
    Graphics::GetGraphicsAPI()->ClearColor(Color(0.0f, 0.0f, 0.0f, 1.0f));
    Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer);
}

void Rendering::Bloom::Run(const RenderingContext& context, Graphics::IFrameBuffer* sceneFrameBuffer)
{
    PINE_PF_SCOPE();

    float threshold = 1.0f;

    if (const auto level = World::GetActiveLevel())
    {
        threshold = level->GetLevelSettings().BloomThreshold;
    }

    // Bright pass: extract HDR brightness above the threshold into the (reduced-res) extract buffer.
    m_ExtractBuffer->Bind();
    Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), Vector2i(BloomWidth(), BloomHeight()));
    Graphics::GetGraphicsAPI()->ClearColor(Color(0.0f, 0.0f, 0.0f, 1.0f));
    Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer);
    Graphics::GetGraphicsAPI()->SetDepthTestEnabled(false);

    m_BloomExtractShader->GetProgram()->Use();

    m_BloomExtractShader->GetProgram()->GetUniformVariable("threshold")->LoadFloat(threshold);

    // Sample the scene the same way the resolve pass does, so the glow stays aligned with the image.
    m_BloomExtractShader->GetProgram()->GetUniformVariable("viewportScale")->LoadVector2(
        Vector2f(context.Size.x / static_cast<float>(sceneFrameBuffer->GetSize().x),
                 context.Size.y / static_cast<float>(sceneFrameBuffer->GetSize().y)));

    sceneFrameBuffer->GetColorBuffer()->Bind(0);

    Common::QuadTarget::Render();

    // Blur the extracted brightness. Blur's first pass reads whatever is bound to texture unit 0.
    m_ExtractBuffer->GetColorBuffer()->Bind();
    Common::Blur::Run(m_BlurContext);
}

Graphics::ITexture* Rendering::Bloom::GetOutputTexture()
{
    return m_BlurContext.TargetBuffer->GetColorBuffer();
}

void Rendering::Bloom::Shutdown()
{
    m_BlurContext.Destroy();

    Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_ExtractBuffer);
}
