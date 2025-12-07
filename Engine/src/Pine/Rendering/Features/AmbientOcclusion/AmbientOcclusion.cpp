#include "AmbientOcclusion.hpp"

#include <random>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Timer/Timer.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Graphics/ShaderStorage/ShaderStorage.hpp"
#include "Pine/Rendering/Common/Blur/Blur.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"
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

    Graphics::ShaderStorage<KernelData> KernelDataStorage(5, "KernelData");

    void CreateRenderBuffer()
    {
        m_RenderBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
        m_RenderBuffer->Prepare();

        const auto renderTargetTexture = Graphics::GetGraphicsAPI()->CreateTexture();

        renderTargetTexture->Bind();
        renderTargetTexture->UploadTextureData(
            Renderer3D::Specifications::General::INTERNAL_WIDTH / AMBIENT_OCCLUSION_RES,
            Renderer3D::Specifications::General::INTERNAL_HEIGHT / AMBIENT_OCCLUSION_RES,
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
        m_KernelRandomnessTexture->UploadTextureData(4, 4, Graphics::TextureFormat::RGB16F, Graphics::TextureDataFormat::Float, &kernelRandomness[0]);
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

}

void Rendering::AmbientOcclusion::Run(const RenderingContext& context)
{
    if (context.SceneCamera == nullptr)
    {
        return;
    }

    Timer AmbientOcclusionTimer;

    m_RenderBuffer->Bind();

    Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), Vector2i(Renderer3D::Specifications::General::INTERNAL_WIDTH / AMBIENT_OCCLUSION_RES, Renderer3D::Specifications::General::INTERNAL_HEIGHT / AMBIENT_OCCLUSION_RES));
    Graphics::GetGraphicsAPI()->ClearColor(Color(0.0f, 0.0f, 0.0f, 1.0f));
    Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer);
    Graphics::GetGraphicsAPI()->SetDepthTestEnabled(false);

    m_AmbientOcclusionShader->GetProgram()->Use();

    // This isn't really necessary, but will allow hot-reload.
    if (!m_AmbientOcclusionShader->IsReady())
    {
        KernelDataStorage.AttachShaderProgram(m_AmbientOcclusionShader->GetProgram());
        m_AmbientOcclusionShader->SetReady(true);
    }

    m_AmbientOcclusionShader->GetProgram()->GetUniformVariable("projectionMatrix")->LoadMatrix4(
        context.SceneCamera->GetProjectionMatrix()
    );

    m_AmbientOcclusionShader->GetProgram()->GetUniformVariable("invProjectionMatrix")->LoadMatrix4(
        glm::inverse(context.SceneCamera->GetProjectionMatrix())
    );

    m_DepthBuffer->GetColorBuffer()->Bind(0);
    m_DepthBuffer->GetDepthBuffer()->Bind(1);
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
    m_AmbientOcclusionShader = Assets::Get<Shader>("engine/shaders/post-processing/ambient-occlusion.shader");

    CreateRenderBuffer();
    CreateKernel();
    CreateKernelRandomnessTexture();

    m_BlurContext.UseSingleChannel = true;
    m_BlurContext.PassCount = 4;
    m_BlurContext.Width = Renderer3D::Specifications::General::INTERNAL_WIDTH / AMBIENT_OCCLUSION_RES;
    m_BlurContext.Height = Renderer3D::Specifications::General::INTERNAL_HEIGHT / AMBIENT_OCCLUSION_RES;

    m_BlurContext.Create();

    m_BlurContext.TargetBuffer->GetColorBuffer()->SetFilteringMode(Graphics::TextureFilteringMode::Linear);
}

void Rendering::AmbientOcclusion::Shutdown()
{
    m_BlurContext.Destroy();

    KernelDataStorage.Dispose();

    Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_RenderBuffer);
    Graphics::GetGraphicsAPI()->DestroyTexture(m_KernelRandomnessTexture);
}
