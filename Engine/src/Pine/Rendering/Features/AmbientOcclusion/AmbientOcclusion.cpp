#include "AmbientOcclusion.hpp"

#include <random>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Timer/Timer.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Graphics/ShaderStorage/ShaderStorage.hpp"
#include "Pine/Rendering/GraphicsSettings/GraphicsSettings.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Common/Blur/Blur.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"
#include "Pine/Rendering/InternalResolution/InternalResolution.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"

using namespace Pine;
using namespace Pine::Renderer3D::Specifications::PostProcessing;

namespace
{
    Shader* m_AmbientOcclusionShader = nullptr;

    Graphics::ITexture* m_KernelRandomnessTexture = nullptr;

    Graphics::IFrameBuffer* m_RenderBuffer = nullptr;
    Graphics::IFrameBuffer* m_DepthBuffer = nullptr;

    Rendering::Common::Blur::BlurContext m_BlurContext;

    struct KernelData
    {
        Vector4f Kernel[64];
    };

    Graphics::ShaderStorage<KernelData> KernelDataStorage(Renderer3D::Specifications::ShaderStorages::AO_DATA, "KernelData");

    // The AO pass covers the whole of its own buffer, so it is sized from the internal resolution
    // rather than from any one context, just divided down for speed. Its *input* is the corner of
    // the pre-pass buffers that the context drew into, which is what viewportScale reaches.
    Vector2i BufferResolution()
    {
        const int divisor = Rendering::GraphicsSettings::GetAmbientOcclusionResDivisor();

        return Rendering::InternalResolution::Get() / divisor;
    }

    void CreateRenderBuffer()
    {
        m_RenderBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
        m_RenderBuffer->Prepare();

        const auto renderTargetTexture = Graphics::GetGraphicsAPI()->CreateTexture();

        const auto resolution = BufferResolution();

        renderTargetTexture->Bind();
        renderTargetTexture->UploadTextureData(
            resolution.x,
            resolution.y,
            0,
            Graphics::TextureFormat::SingleChannel,
            Graphics::TextureDataFormat::Float,
            nullptr);

        m_RenderBuffer->AttachTexture(renderTargetTexture, Graphics::BufferAttachment::Color);
        m_RenderBuffer->Finish();
    }

    void CreateKernelRandomnessTexture()
    {
        std::vector<Vector3f> kernelRandomness;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution randomFloat(0.f, 1.f);

        kernelRandomness.reserve(16);

        for (int i = 0; i < 16; i++)
        {
            kernelRandomness.emplace_back(
                randomFloat(gen) * 2.0f - 1.0f, // [-1, 1]
                randomFloat(gen) * 2.0f - 1.0f, // [-1, 1]
                0.f
            );
        }

        m_KernelRandomnessTexture = Graphics::GetGraphicsAPI()->CreateTexture();
        m_KernelRandomnessTexture->Bind();
        m_KernelRandomnessTexture->SetTextureWrapMode(Graphics::TextureWrapMode::Repeat);
        m_KernelRandomnessTexture->SetFilteringMode(Graphics::TextureFilteringMode::Nearest);
        m_KernelRandomnessTexture->UploadTextureData(4, 4, 0, Graphics::TextureFormat::RGB16F, Graphics::TextureDataFormat::Float, &kernelRandomness[0]);
    }

    void CreateKernel()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution randomFloat(0.f, 1.f);

        for (int i = 0; i < 64; i++)
        {
            auto& sample = KernelDataStorage.Data().Kernel[i];

            sample =
            {
                randomFloat(gen) * 2.0f - 1.0f, // [-1, 1]
                randomFloat(gen) * 2.0f - 1.0f, // [-1, 1]
                randomFloat(gen), // [0, 1],
                0.f // We don't use this, but we need to align with std140.
            };

            sample = glm::normalize(sample);
            sample *= randomFloat(gen);

            const float scale = static_cast<float>(i) / 64.f;

            sample *= Math::LinearInterpolation(0.1f, 1.f, scale * scale);
        }

        m_AmbientOcclusionShader->GetProgram()->Use();

        KernelDataStorage.Create();
        KernelDataStorage.AttachShaderProgram(m_AmbientOcclusionShader->GetProgram());
        KernelDataStorage.Upload();
    }

    void CreateBuffers()
    {
        CreateRenderBuffer();

        const auto resolution = BufferResolution();

        m_BlurContext.UseSingleChannel = true;
        m_BlurContext.PassCount = Rendering::GraphicsSettings::GetAmbientOcclusionBlurPasses();
        m_BlurContext.Width = resolution.x;
        m_BlurContext.Height = resolution.y;

        m_BlurContext.Create();

        m_BlurContext.TargetBuffer->GetColorBuffer()->SetFilteringMode(Graphics::TextureFilteringMode::Linear);

        // Clear the output to white (1.0 = no occlusion) so that if the pass is
        // disabled (or hasn't run yet) the post-process AO multiply is a no-op
        // instead of darkening the whole scene to black.
        m_BlurContext.TargetBuffer->Bind();
        Graphics::GetGraphicsAPI()->ClearColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer);
    }

    void DestroyBuffers()
    {
        m_BlurContext.Destroy();

        Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_RenderBuffer);

        m_RenderBuffer = nullptr;
    }

}

void Rendering::AmbientOcclusion::Run(const RenderingContext& context)
{
    PINE_PF_SCOPE();

    if (context.SceneCamera == nullptr)
    {
        return;
    }

    Timer AmbientOcclusionTimer;

    m_RenderBuffer->Bind();

    Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), BufferResolution());

    // Blur pass count is a live setting (no allocation), refresh it each frame.
    m_BlurContext.PassCount = Rendering::GraphicsSettings::GetAmbientOcclusionBlurPasses();
    Graphics::GetGraphicsAPI()->ClearColor(Color(0.0f, 0.0f, 0.0f, 1.0f));
    Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer);
    Graphics::GetGraphicsAPI()->SetDepthTestEnabled(false);

    m_AmbientOcclusionShader->GetProgram()->Use();

    // This isn't really necessary, but will allow hot-reload.
    if (!m_AmbientOcclusionShader->IsRendererReady())
    {
        KernelDataStorage.AttachShaderProgram(m_AmbientOcclusionShader->GetProgram());
        m_AmbientOcclusionShader->SetRendererReady(true);
    }

    m_AmbientOcclusionShader->GetProgram()->GetUniformVariable("projectionMatrix")->LoadMatrix4(
        context.SceneCamera->GetProjectionMatrix()
    );

    m_AmbientOcclusionShader->GetProgram()->GetUniformVariable("invProjectionMatrix")->LoadMatrix4(
        glm::inverse(context.SceneCamera->GetProjectionMatrix())
    );

    m_AmbientOcclusionShader->GetProgram()->GetUniformVariable("sampleCount")->LoadInteger(
        Rendering::GraphicsSettings::GetAmbientOcclusionSamples()
    );

    // The pre-pass fills the context-sized corner of buffers allocated at the internal resolution,
    // the same arrangement the scene buffer has, so every lookup into them is scaled the way
    // PostProcessing scales its own.
    //
    // Against the internal resolution rather than the buffer's own reported size: the pre-pass
    // buffer is built attachment by attachment, and IFrameBuffer::GetSize only knows the size of a
    // buffer that allocated its attachments in one call, so it reports 0 here.
    const auto allocatedResolution = Rendering::InternalResolution::Get();

    m_AmbientOcclusionShader->GetProgram()->GetUniformVariable("viewportScale")->LoadVector2(
        Vector2f(context.Size.x / static_cast<float>(allocatedResolution.x),
                 context.Size.y / static_cast<float>(allocatedResolution.y))
    );

    m_DepthBuffer->GetColorBuffer()->Bind(0);

    // Depth-stencil, because the pre-pass buffer carries the packed format its depth has to be in
    // to be blitted into the scene buffer. Sampling it reads the depth component.
    m_DepthBuffer->GetDepthStencilBuffer()->Bind(1);
    m_KernelRandomnessTexture->Bind(2);

    Common::QuadTarget::Render();

    m_RenderBuffer->GetColorBuffer()->Bind();

    Common::Blur::Run(m_BlurContext);

    AmbientOcclusionTimer.Stop();
}

Graphics::ITexture * Rendering::AmbientOcclusion::GetOutputTexture()
{
    return m_BlurContext.TargetBuffer->GetColorBuffer();
}

void Rendering::AmbientOcclusion::UseDepthBuffer(Graphics::IFrameBuffer *buffer)
{
    m_DepthBuffer = buffer;
}

void Rendering::AmbientOcclusion::Setup()
{
    m_AmbientOcclusionShader = Assets::Get<Shader>("engine/shaders/post-processing/ambient-occlusion");

    assert(m_AmbientOcclusionShader != nullptr);

    CreateBuffers();
    CreateKernel();
    CreateKernelRandomnessTexture();

    Rendering::InternalResolution::AddResizeCallback([]
    {
        DestroyBuffers();
        CreateBuffers();
    });
}

void Rendering::AmbientOcclusion::Shutdown()
{
    DestroyBuffers();

    KernelDataStorage.Dispose();

    Graphics::GetGraphicsAPI()->DestroyTexture(m_KernelRandomnessTexture);
}
