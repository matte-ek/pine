#include "Blur.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"

using namespace Pine;

namespace
{
    Shader* m_BlurShader = nullptr;

    Graphics::IFrameBuffer* CreateBuffer(const int width, const int height, const bool singleChannel, const bool hdr)
    {
        const auto buffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();

        buffer->Prepare();

        const auto blurTargetTexture = Graphics::GetGraphicsAPI()->CreateTexture();

        const auto format = hdr ? Graphics::TextureFormat::RGBA16F
                                : (singleChannel ? Graphics::TextureFormat::SingleChannel : Graphics::TextureFormat::RGBA);
        const auto dataFormat = hdr ? Graphics::TextureDataFormat::Float : Graphics::TextureDataFormat::UnsignedByte;

        blurTargetTexture->Bind();
        blurTargetTexture->UploadTextureData(
            width, height,
            0,
            format,
            dataFormat,
            nullptr);

        // Blur taps outside the image must stay at the edge instead of wrapping across it.
        blurTargetTexture->SetTextureWrapMode(Graphics::TextureWrapMode::ClampToEdge);

        buffer->AttachTexture(blurTargetTexture, Graphics::BufferAttachment::Color);
        buffer->Finish();

        return buffer;
    }

    void DoBlurPass(const int offset, Graphics::IFrameBuffer* targetOne, Graphics::IFrameBuffer* targetTwo)
    {
        const bool sourceSwap = offset % 2 == 0;

        Graphics::IFrameBuffer* targetBuffer = sourceSwap ? targetTwo : targetOne;

        if (offset != 1)
        {
            Graphics::IFrameBuffer* sourceBuffer = sourceSwap ? targetOne : targetTwo;
            sourceBuffer->GetColorBuffer()->Bind();
        }

        targetBuffer->Bind();

        Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer);

        m_BlurShader->GetProgram()->GetUniformVariable("offset")->LoadInteger(offset);

        Rendering::Common::QuadTarget::Render();
    }
}

void Rendering::Common::Blur::BlurContext::Create()
{
    IntermediateBuffer = CreateBuffer(Width, Height, UseSingleChannel, UseHDR);
    TargetBuffer = CreateBuffer(Width, Height, UseSingleChannel, UseHDR);
}

void Rendering::Common::Blur::BlurContext::Destroy()
{
    Graphics::GetGraphicsAPI()->DestroyFrameBuffer(IntermediateBuffer);
    Graphics::GetGraphicsAPI()->DestroyFrameBuffer(TargetBuffer);

    IntermediateBuffer = nullptr;
    TargetBuffer = nullptr;
}

void Rendering::Common::Blur::Setup()
{
    m_BlurShader = Assets::Get<Shader>("engine/shaders/post-processing/blur");
}

void Rendering::Common::Blur::Shutdown()
{
}

void Rendering::Common::Blur::Run(const BlurContext& context)
{
    Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), Vector2i(context.Width, context.Height));
    Graphics::GetGraphicsAPI()->ClearColor(Color(0.0f, 0.0f, 0.0f, 1.0f));

    assert(m_BlurShader != nullptr);

    m_BlurShader->GetProgram()->Use();

    m_BlurShader->GetProgram()->GetUniformVariable("texelSize")->LoadVector2(
        Vector2f(1.f) / Vector2f(context.Width, context.Height)
    );

    const bool swapTarget = context.PassCount % 2 == 0;

    for (int i = 1; i <= context.PassCount;i++)
    {
        DoBlurPass(i, swapTarget ? context.IntermediateBuffer : context.TargetBuffer,
                      swapTarget ? context.TargetBuffer : context.IntermediateBuffer);
    }
}
