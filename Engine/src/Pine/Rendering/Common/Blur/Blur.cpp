#include "Blur.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"

using namespace Pine;

namespace
{
    Shader* m_BlurShader = nullptr;

    Graphics::IFrameBuffer* CreateBuffer(int width, int height, bool singleChannel)
    {
        const auto buffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();

        buffer->Prepare();

        const auto blurTargetTexture = Graphics::GetGraphicsAPI()->CreateTexture();

        blurTargetTexture->Bind();
        blurTargetTexture->UploadTextureData(
            width, height,
            singleChannel ? Graphics::TextureFormat::SingleChannel : Graphics::TextureFormat::RGBA,
            Graphics::TextureDataFormat::UnsignedByte,
            nullptr);

        buffer->AttachTexture(blurTargetTexture, Graphics::BufferAttachment::Color);
        buffer->Finish();

        return buffer;
    }

    void DoBlurPass(int offset, Graphics::IFrameBuffer* targetOne, Graphics::IFrameBuffer* targetTwo)
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
    IntermediateBuffer = CreateBuffer(Width, Height, UseSingleChannel);
    TargetBuffer = CreateBuffer(Width, Height, UseSingleChannel);
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
    m_BlurShader = Assets::Get<Shader>("engine/shaders/post-processing/blur.shader");
}

void Rendering::Common::Blur::Shutdown()
{
}

void Rendering::Common::Blur::Run(const BlurContext& context)
{
    Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), Vector2i(context.Width, context.Height));
    Graphics::GetGraphicsAPI()->ClearColor(Color(0.0f, 0.0f, 0.0f, 1.0f));

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
