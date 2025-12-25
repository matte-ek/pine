#include "Renderer3D.hpp"

#include "../RenderingContext.hpp"
#include "Specifications.hpp"
#include "ShaderStorages.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;

namespace
{
    Renderer3D::RenderConfiguration m_RenderingConfiguration;
    RenderingContext* m_RenderingContext = nullptr;

    // Cached graphics API for the current context
    Graphics::IGraphicsAPI* m_GraphicsAPI = nullptr;

    // This default texture is a solid white pixel that we treat as a "no-texture" texture.
    Graphics::ITexture* m_DefaultTexture = nullptr;

    Graphics::IShaderProgram* m_Shader = nullptr;
    ShaderVersion m_ShaderVersion = ShaderVersion::Default;

    Graphics::IUniformVariable* m_HasTangentData = nullptr;
    Graphics::IUniformVariable* m_HasDirectionalShadowMapUniform = nullptr;

    bool m_HasDirectionalShadowMap = false;
    Graphics::ITexture* m_DirectionalShadowMap = nullptr;

    Mesh* m_Mesh = nullptr;

    int m_CurrentInstanceIndex = 0;

    Material* m_Material = nullptr;

    Camera* m_Camera = nullptr;

    int m_CurrentLightIndex = 0;
}

void Renderer3D::Setup()
{
    m_GraphicsAPI = Graphics::GetGraphicsAPI();

    m_DefaultTexture = m_GraphicsAPI->CreateTexture();

    // Create a 1x1 solid white pixel texture
    auto* textureData = static_cast<std::uint8_t*>(malloc(sizeof(std::uint8_t) * 4));

    for (size_t i = 0; i < sizeof(std::uint8_t) * 4;i++)
        textureData[i] = 255;

    m_DefaultTexture->Bind();
    m_DefaultTexture->UploadTextureData(1, 1, Graphics::TextureFormat::RGBA, Graphics::TextureDataFormat::UnsignedByte, textureData);

    free(textureData);

    ShaderStorages::Matrix.Create();
    ShaderStorages::Instance.Create();
    ShaderStorages::Material.Create();
    ShaderStorages::Lights.Create();
    ShaderStorages::Shadows.Create();
}

void Renderer3D::Shutdown()
{
    ShaderStorages::Matrix.Dispose();
    ShaderStorages::Instance.Dispose();
    ShaderStorages::Material.Dispose();
    ShaderStorages::Lights.Dispose();
    ShaderStorages::Shadows.Dispose();
}

Renderer3D::RenderConfiguration& Renderer3D::GetRenderConfiguration()
{
    return m_RenderingConfiguration;
}

void Renderer3D::PrepareMesh(Mesh *mesh, Material* overrideMaterial)
{
    mesh->GetVertexArray()->Bind();

    m_CurrentInstanceIndex = 0;
    m_Mesh = mesh;

    if (m_RenderingConfiguration.SkipMaterialInitialization)
    {
        if (m_RenderingConfiguration.OverrideShader)
        {
            SetShader(m_RenderingConfiguration.OverrideShader);
        }

        return;
    }

    // ehh
    m_Material = m_RenderingConfiguration.OverrideMaterial ? m_RenderingConfiguration.OverrideMaterial : overrideMaterial ? overrideMaterial : mesh->GetMaterial();

    if (!m_Material)
    {
        return;
    }

    auto version = ShaderVersion::Default;

    if (m_Material->GetRenderingMode() == MaterialRenderingMode::Discard)
    {
        version = ShaderVersion::Discard;
    }

    const auto shader = m_RenderingConfiguration.OverrideShader ? m_RenderingConfiguration.OverrideShader : m_Material->GetShader();
    if (shader->GetProgram() != m_Shader ||
        m_ShaderVersion != version ||
        !shader->IsReady())
    {
        SetShader(shader, version);
    }

    if (!m_Shader)
    {
        return;
    }

    // Apply shadow data
    if (m_HasDirectionalShadowMapUniform)
    {
        m_HasDirectionalShadowMapUniform->LoadInteger(m_HasDirectionalShadowMap);

        if (m_HasDirectionalShadowMap && m_DirectionalShadowMap)
        {
            m_DirectionalShadowMap->Bind(Specifications::Samplers::DIRECTIONAL_SHADOW_MAP);
        }
    }

    // Apply Textures

    // Diffuse
    if (m_Material->GetDiffuse())
        m_Material->GetDiffuse()->GetGraphicsTexture()->Bind(Specifications::Samplers::DIFFUSE);
    else
        m_DefaultTexture->Bind(Specifications::Samplers::DIFFUSE);

    // Specular
    if (m_Material->GetSpecular())
        m_Material->GetSpecular()->GetGraphicsTexture()->Bind(Specifications::Samplers::SPECULAR);
    else
        m_DefaultTexture->Bind(Specifications::Samplers::SPECULAR);

    // Normal
    if (m_Material->GetNormal())
        m_Material->GetNormal()->GetGraphicsTexture()->Bind(Specifications::Samplers::NORMAL);
    else
        m_DefaultTexture->Bind(Specifications::Samplers::NORMAL);

    /* Material Properties */
    auto& materialData = ShaderStorages::Material.Data();

    materialData.DiffuseColor = m_Material->GetDiffuseColor();
    materialData.SpecularColor = m_Material->GetSpecularColor();
    materialData.AmbientColor = m_Material->GetAmbientColor();
    materialData.Shininess = m_Material->GetShininess();
    materialData.UVScale = m_Material->GetTextureScale();

    ShaderStorages::Material.Upload();

    if (m_HasTangentData)
    {
        m_HasTangentData->LoadInteger(m_Material->GetNormal() != nullptr);
    }
}

bool Renderer3D::AddInstance(const Matrix4f& transformationMatrix, ModelRendererHintData* data)
{
    const bool isFull = m_CurrentInstanceIndex == Specifications::General::MAX_INSTANCE_COUNT - 1;
    const int instanceId = m_CurrentInstanceIndex++;

    ShaderStorages::Instance.Data().Instances[instanceId].TransformationMatrix = transformationMatrix;

    if (data != nullptr)
    {
        auto& lightIndices = ShaderStorages::Instance.Data().Instances[instanceId].LightIndices;

        for (int i = 0; i < 4;i++)
        {
            if (auto light = data->LightSlotIndex[i].Get())
            {
                lightIndices[i] = light->GetLightHintData().LightIndex;
            }
        }
    }

    return isFull;
}

void Renderer3D::RenderMesh(const Matrix4f& transformationMatrix, ModelRendererHintData* data, int writeStencilBuffer)
{
    if (data != nullptr)
    {
        auto& lightIndices = ShaderStorages::Instance.Data().Instances[0].LightIndices;

        for (int i = 0; i < 4;i++)
        {
            if (auto light = data->LightSlotIndex[i].Get())
            {
                lightIndices[i] = light->GetLightHintData().LightIndex;
            }
        }
    }

    ShaderStorages::Instance.Data().Instances[0].TransformationMatrix = transformationMatrix;
    ShaderStorages::Instance.Upload(sizeof(ShaderStorages::InstanceData::Instance));

    if (writeStencilBuffer != 0)
    {
        m_GraphicsAPI->SetStencilOperation(Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep, Graphics::StencilOperation::Replace);
        m_GraphicsAPI->SetStencilFunction(Graphics::TestFunction::Always, writeStencilBuffer, 0x0);
    }

    if (m_Mesh->HasElementBuffer())
    {
        m_GraphicsAPI->DrawElements(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount());
    }
    else
    {
        m_GraphicsAPI->DrawArrays(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount());
    }

    if (m_RenderingContext != nullptr)
    {
        m_RenderingContext->DrawCalls++;
    }

    if (writeStencilBuffer != 0)
    {
        m_GraphicsAPI->SetStencilOperation(Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep);
        m_GraphicsAPI->SetStencilFunction(Graphics::TestFunction::Always, 0x0, 0x0);
    }
}

void Renderer3D::RenderMeshInstanced()
{
    if (m_CurrentInstanceIndex == 0)
    {
        return;
    }

    ShaderStorages::Instance.Upload(sizeof(ShaderStorages::InstanceData::Instance) * m_CurrentInstanceIndex);

    if (m_Mesh->HasElementBuffer())
    {
        m_GraphicsAPI->DrawElementsInstanced(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount(), m_CurrentInstanceIndex);
    }
    else
    {
        m_GraphicsAPI->DrawArraysInstanced(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount(), m_CurrentInstanceIndex);
    }

    if (m_RenderingContext != nullptr)
    {
        m_RenderingContext->DrawCalls++;
    }

    m_CurrentInstanceIndex = 0;
}

void Renderer3D::SetShader(Shader* shader, const ShaderVersion preferredVersion)
{
    // Figure out what version of the shader to use
    ShaderVersion version = preferredVersion;

    if (!m_RenderingConfiguration.IgnoreShaderVersions)
    {
        if (!shader->HasShaderVersion(preferredVersion))
        {
            Log::Warning("Requested shader version not found, rendering may be affected.");
            version = ShaderVersion::Default;
        }
    }
    else
    {
        version = ShaderVersion::Default;
    }

    const auto shaderProgram = shader->GetProgram(version);

    // Make sure the renderer's shader storages has been set up properly.
    // TODO: Future vision, this "is ready" crap should probably be handled by the renderer or something instead.
    if (!shader->IsReady(version))
    {
        if (!ShaderStorages::Matrix.AttachShaderProgram(shaderProgram))
        {
            Log::Error("Renderer3D: Shader is missing 'Matrix' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Instance.AttachShaderProgram(shaderProgram))
        {
            Log::Error("Renderer3D: Shader is missing 'Transform' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Material.AttachShaderProgram(shaderProgram))
        {
            Log::Error("Renderer3D: Shader is missing 'Material' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Lights.AttachShaderProgram(shaderProgram))
        {
            Log::Error("Renderer3D: Shader is missing 'Lights' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Shadows.AttachShaderProgram(shaderProgram))
        {
            Log::Error("Renderer3D: Shader is missing 'Shadows' shader storage, expect rendering issues.");
        }

        shader->SetReady(true, version);
    }

    m_Shader = shaderProgram;
    m_Shader->Use();

    m_ShaderVersion = version;

    m_HasTangentData = m_Shader->GetUniformVariable("hasTangentData");
    m_HasDirectionalShadowMapUniform = m_Shader->GetUniformVariable("hasDirectionalShadowMap");
}

void Renderer3D::SetAmbientColor(Vector3f ambientColor)
{
    ShaderStorages::Lights.Data().AmbientColor = ambientColor;
}

void Renderer3D::UseRenderingContext(RenderingContext *renderingContext)
{
    m_RenderingContext = renderingContext;
}

void Renderer3D::SetCamera(Camera* camera)
{
    m_Camera = camera;

    ShaderStorages::Matrix.Data().Projection = camera->GetProjectionMatrix();
    ShaderStorages::Matrix.Data().View = camera->GetViewMatrix();

    ShaderStorages::Matrix.Upload();
}

void Renderer3D::SetCamera(const Matrix4f &viewMatrix, const Matrix4f &projMatrix)
{
    m_Camera = nullptr;

    ShaderStorages::Matrix.Data().Projection = projMatrix;
    ShaderStorages::Matrix.Data().View = viewMatrix;

    ShaderStorages::Matrix.Upload();
}

Camera *Renderer3D::GetCamera()
{
    return m_Camera;
}

void Renderer3D::AddLight(Light *light)
{
    if (m_CurrentLightIndex >= Specifications::General::DYNAMIC_LIGHT_COUNT)
    {
        Log::Warning("Maximum number of dynamic lights reached.");

        return;
    }

    const auto rotation = -normalize(rotate(light->GetParent()->GetTransform()->GetRotation(), Vector3f(0.f, 0.f, -1.f)));
    const int lightSlot = light->GetLightType() == LightType::Directional ? 0 : m_CurrentLightIndex++;
    auto& lightData = ShaderStorages::Lights.Data().Lights[lightSlot];

    lightData.Position = light->GetParent()->GetTransform()->GetPosition();
    lightData.Rotation = rotation;
    lightData.Color = light->GetLightColor();
    lightData.Attenuation = light->GetLightAttenuation();
    lightData.Angle = light->GetSpotlightRadius();
    lightData.AngleSmoothness = light->GetSpotlightCutoff();

    light->GetLightHintData().LightIndex = lightSlot;

    auto& [_, LightIndices] = ShaderStorages::Instance.Data().Instances[0];

    if (light->GetLightType() == LightType::PointLight)
    {
        if (LightIndices.x == 0)
        {
            LightIndices.x = lightSlot;
        }
        else if (LightIndices.y == 0)
        {
            LightIndices.y = lightSlot;
        }
        else
        {
            LightIndices.z = lightSlot;
        }
    }
    else if (light->GetLightType() == LightType::SpotLight)
    {
        LightIndices.w = lightSlot;
    }
}

void Renderer3D::UploadLights()
{
    ShaderStorages::Lights.Upload();

    m_CurrentLightIndex = 1;
}

void Renderer3D::AddDirectionalShadowMap(Graphics::ITexture *depthMap)
{
    auto& lightData = ShaderStorages::Lights.Data();

    m_DirectionalShadowMap = depthMap;
    m_HasDirectionalShadowMap = true;
}

void Renderer3D::FrameReset()
{
    for (auto& Light : ShaderStorages::Lights.Data().Lights)
    {
        Light.Color = Vector3f(0.0f, 0.0f, 0.0f);
    }

    ShaderStorages::Lights.Data().AmbientColor = Vector3f(0.f);

    m_RenderingContext = nullptr;
    m_Shader = nullptr;
    m_Mesh = nullptr;
    m_Material = nullptr;
    m_HasDirectionalShadowMap = false;

    m_RenderingConfiguration.OverrideShader = nullptr;
    m_RenderingConfiguration.OverrideMaterial = nullptr;
    m_RenderingConfiguration.IgnoreShaderVersions = false;
    m_RenderingConfiguration.SkipMaterialInitialization = false;
}
