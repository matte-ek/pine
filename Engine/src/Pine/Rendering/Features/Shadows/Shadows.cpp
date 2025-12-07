#include "Shadows.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer3D/ShaderStorages.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;
using namespace Renderer3D::Specifications::Shadows;

namespace
{
    Camera* m_SceneCamera = nullptr;
    Shader* m_ShadowShader = nullptr;

    Graphics::IFrameBuffer* m_DirectionalShadowMapBuffer = nullptr;

    Vector3f ComputeBoxCenter(const std::array<Vector3f, 8>& corners)
    {
        auto min = Vector3f(std::numeric_limits<float>::max());
        auto max = Vector3f(std::numeric_limits<float>::lowest());

        for (const auto& corner : corners)
        {
            min = glm::min(min, corner);
            max = glm::max(max, corner);
        }

        return (min + max) / 2.f;
    }

    Matrix4f BuildViewMatrix(const std::array<Vector3f, 8>& corners, Vector3f lightDirection)
    {
        const auto center = ComputeBoxCenter(corners);

        return glm::lookAt(center - lightDirection, center, Vector3f(0.f, 1.f, 0.f));
    }

    Matrix4f BuildProjectionMatrix(std::array<Vector3f, 8> corners, const Matrix4f &viewMatrix, float farPlaneMargin)
    {
        for (auto& corner : corners)
        {
            corner = viewMatrix * Vector4f(corner, 1.f);
        }

        auto min = Vector3f(std::numeric_limits<float>::max());
        auto max = Vector3f(std::numeric_limits<float>::lowest());

        for (const auto& corner : corners)
        {
            min = glm::min(min, corner);
            max = glm::max(max, corner);
        }

        return glm::ortho(min.x, max.x, min.y, max.y, min.z - farPlaneMargin, max.z + farPlaneMargin);
    }

    void RenderScene(const Pipeline3D::ObjectBatchMap& mapBatch)
    {
        for (const auto& [modelGroup, objectRenderInstances] : mapBatch)
        {
            const auto model = modelGroup.Model;

            int meshIndex = -1;
            for (const auto mesh : model->GetMeshes())
            {
                meshIndex++;

                Renderer3D::PrepareMesh(mesh);

                for (const auto [renderer, distance] : objectRenderInstances)
                {
                    const auto modelRenderer = renderer;

                    modelRenderer->GetParent()->GetTransform()->OnRender(0.f);

                    if (const int modelMeshIndex = modelRenderer->GetModelMeshIndex(); modelMeshIndex >= 0)
                    {
                        if (modelMeshIndex != meshIndex)
                        {
                            continue;
                        }
                    }

                    if (Renderer3D::AddInstance(modelRenderer->GetParent()->GetTransform()->GetTransformationMatrix()))
                    {
                        Renderer3D::RenderMeshInstanced();
                    }
                }

                Renderer3D::RenderMeshInstanced();
            }
        }
    }

    void BuildLightSpaceMatrices(Vector3f lightDirection)
    {
        const float oldNearPane = m_SceneCamera->GetNearPlane();
        const float oldFarPlane = m_SceneCamera->GetFarPlane();

        const std::array<float, CASCADE_COUNT> farPlane = { 5.f, 10.f, 30.f, m_SceneCamera->GetFarPlane() };

        auto& shadowData = Renderer3D::ShaderStorages::Shadows.Data();

        for (int i = 0; i < CASCADE_COUNT; i++)
        {
            if (i != 0)
                m_SceneCamera->SetNearPlane(oldNearPane + farPlane[i - 1]);

            m_SceneCamera->SetFarPlane(farPlane[i]);
            m_SceneCamera->OnRender(0.f);

            const auto frustumCorners = m_SceneCamera->GetFrustumCorners();

            const auto viewMatrix = BuildViewMatrix(frustumCorners, lightDirection);
            const auto projectionMatrix = BuildProjectionMatrix(frustumCorners, viewMatrix, farPlane[i] * 0.5f);

            shadowData.LightSpaceMatrix[i] = projectionMatrix * viewMatrix;
        }

        Renderer3D::ShaderStorages::Shadows.Upload();

        m_SceneCamera->SetNearPlane(oldNearPane);
        m_SceneCamera->SetFarPlane(oldFarPlane);
        m_SceneCamera->OnRender(0.f);
    }

    void HandleDirectionalShadowMap(const Light* light, const Pipeline3D::ObjectBatchData &batchData)
    {
        const auto context = RenderManager::GetCurrentRenderingContext();
        auto& renderSettings = Renderer3D::GetRenderConfiguration();

        if (m_SceneCamera == nullptr)
        {
            return;
        }

        // Prepare frame buffer
        m_DirectionalShadowMapBuffer->Bind();

        Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);
        Graphics::GetGraphicsAPI()->SetFaceCullingMode(Graphics::FaceCullMode::Front);
        Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), Vector2i(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION));
        Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::DepthBuffer);

        const auto direction = light->GetParent()->GetTransform()->GetRotation() * Vector3f(0.f, 0.f, -1.f);

        BuildLightSpaceMatrices(direction);

        // Prepare Renderer3D
        Renderer3D::FrameReset();
        Renderer3D::UseRenderingContext(context);

        renderSettings.OverrideShader = m_ShadowShader;
        renderSettings.IgnoreShaderVersions = true;
        renderSettings.SkipMaterialInitialization = true;

        // Render scene
        RenderScene(batchData.OpaqueObjects);

        // Restore Renderer3D
        renderSettings.OverrideShader = nullptr;
        renderSettings.IgnoreShaderVersions = false;
        renderSettings.SkipMaterialInitialization = false;

        Graphics::GetGraphicsAPI()->SetFaceCullingMode(Graphics::FaceCullMode::Back);
    }
}

void Rendering::Shadows::Setup()
{
    m_DirectionalShadowMapBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();

    m_DirectionalShadowMapBuffer->Bind();
    m_DirectionalShadowMapBuffer->Prepare();

    const auto depthArrayTexture = Graphics::GetGraphicsAPI()->CreateTexture();

    depthArrayTexture->SetType(Graphics::TextureType::Texture2DArray);
    depthArrayTexture->SetArraySize(CASCADE_COUNT);

    depthArrayTexture->Bind();
    depthArrayTexture->UploadTextureData(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, Graphics::TextureFormat::Depth, Graphics::TextureDataFormat::Float, nullptr);
    depthArrayTexture->SetFilteringMode(Graphics::TextureFilteringMode::Linear);
    depthArrayTexture->SetCompareModeLowerEqual();

    m_DirectionalShadowMapBuffer->AttachTexture(depthArrayTexture, Graphics::BufferAttachment::Depth);
    m_DirectionalShadowMapBuffer->Finish();

    m_ShadowShader = Assets::Get<Shader>("engine/shaders/3d/shadow.shader");
}

void Rendering::Shadows::Shutdown()
{
    Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_DirectionalShadowMapBuffer);
}

void Rendering::Shadows::NewFrame(Camera* sceneCamera)
{
    m_SceneCamera = sceneCamera;
}

void Rendering::Shadows::RenderPassLight(const Light* light, const Pipeline3D::ObjectBatchData &batchData)
{
    Graphics::GetGraphicsAPI()->SetBlendingEnabled(false);

    if (light->GetLightType() == LightType::Directional)
    {
        HandleDirectionalShadowMap(light, batchData);
    }
}

void Rendering::Shadows::UploadShadowData(const Light *light)
{
    if (light->GetLightType() == LightType::Directional)
    {
        Renderer3D::AddDirectionalShadowMap(m_DirectionalShadowMapBuffer->GetDepthBuffer());
    }
}

Graphics::ITexture * Rendering::Shadows::GetShadowMap(const Light *light)
{
    if (light->GetLightType() == LightType::Directional)
    {
        return m_DirectionalShadowMapBuffer->GetDepthBuffer();
    }

    return nullptr;
}