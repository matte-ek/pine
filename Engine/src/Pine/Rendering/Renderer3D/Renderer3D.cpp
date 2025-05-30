#include "Renderer3D.hpp"

#include "../RenderingContext.hpp"
#include "Specifications.hpp"
#include "ShaderStorages.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    Pine::Renderer3D::RenderConfiguration m_RenderingConfiguration;
    Pine::RenderingContext* m_RenderingContext = nullptr;

    // Cached graphics API for the current context
    Pine::Graphics::IGraphicsAPI* m_GraphicsAPI = nullptr;

    // This default texture is a solid white pixel that we treat as a "no-texture" texture.
    Pine::Graphics::ITexture* m_DefaultTexture = nullptr;

    Pine::Graphics::IShaderProgram* m_Shader = nullptr;
    Pine::ShaderVersion m_ShaderVersion = Pine::ShaderVersion::Default;

    Pine::Graphics::IUniformVariable* m_LightIndices = nullptr;
    Pine::Graphics::IUniformVariable* m_HasTangentData = nullptr;

    Pine::Vector4i m_LightIndicesData;

    Pine::Mesh* m_Mesh = nullptr;

    int m_CurrentInstanceIndex = 0;

    Pine::Material* m_Material = nullptr;

    Pine::Camera* m_Camera = nullptr;

    int m_CurrentLightIndex = 0;
}

void Pine::Renderer3D::Setup()
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
    ShaderStorages::Transform.Create();
    ShaderStorages::Material.Create();
    ShaderStorages::Lights.Create();
}

void Pine::Renderer3D::Shutdown()
{
    ShaderStorages::Matrix.Dispose();
    ShaderStorages::Transform.Dispose();
    ShaderStorages::Material.Dispose();
    ShaderStorages::Lights.Dispose();
}

Pine::Renderer3D::RenderConfiguration& Pine::Renderer3D::GetRenderConfiguration()
{
    return m_RenderingConfiguration;
}

void Pine::Renderer3D::PrepareMesh(Mesh *mesh, Material* overrideMaterial)
{
    mesh->GetVertexArray()->Bind();

    m_CurrentInstanceIndex = 0;
    m_Mesh = mesh;

    if (!m_Camera)
    {
        return;
    }

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
        m_HasTangentData->LoadInteger(m_Material->GetNormal() != nullptr);
}

bool Pine::Renderer3D::AddInstance(const Matrix4f&transformationMatrix)
{
    bool isFull = m_CurrentInstanceIndex == Specifications::General::MAX_INSTANCE_COUNT - 1;

    ShaderStorages::Transform.Data().TransformationMatrix[m_CurrentInstanceIndex++] = transformationMatrix;

    return !isFull;
}

void Pine::Renderer3D::RenderMesh(const Matrix4f& transformationMatrix, int writeStencilBuffer)
{
    ShaderStorages::Transform.Data().TransformationMatrix[0] = transformationMatrix;
    ShaderStorages::Transform.Upload(sizeof(Matrix4f));

    if (writeStencilBuffer != 0)
    {
        m_GraphicsAPI->SetStencilOperation(Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep, Graphics::StencilOperation::Replace);
        m_GraphicsAPI->SetStencilFunction(Graphics::TestFunction::Always, writeStencilBuffer, 0x0);
    }

    if (m_LightIndices != nullptr)
    {
        m_LightIndices->LoadVector4(m_LightIndicesData);
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
        m_RenderingContext->DrawCalls++;

    if (writeStencilBuffer != 0)
    {
        m_GraphicsAPI->SetStencilOperation(Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep);
        m_GraphicsAPI->SetStencilFunction(Graphics::TestFunction::Always, 0x0, 0x0);
    }
}

void Pine::Renderer3D::RenderMeshInstanced()
{
    if (m_CurrentInstanceIndex == 0)
        return;

    ShaderStorages::Transform.Upload(sizeof(Matrix4f) * m_CurrentInstanceIndex);

    if (m_LightIndices != nullptr)
    {
        m_LightIndices->LoadVector4(m_LightIndicesData);
    }

    if (m_Mesh->HasElementBuffer())
    {
        m_GraphicsAPI->DrawElementsInstanced(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount(), m_CurrentInstanceIndex);
    }
    else
    {
        m_GraphicsAPI->DrawArraysInstanced(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount(), m_CurrentInstanceIndex);
    }

    if (m_RenderingContext != nullptr)
        m_RenderingContext->DrawCalls++;

    m_CurrentInstanceIndex = 0;
}

void Pine::Renderer3D::SetShader(Shader* shader, const ShaderVersion preferredVersion)
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

        if (!ShaderStorages::Transform.AttachShaderProgram(shaderProgram))
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

        shader->SetReady(true, version);
    }

    m_Shader = shaderProgram;
    m_Shader->Use();

    m_ShaderVersion = version;

    m_HasTangentData = m_Shader->GetUniformVariable("hasTangentData");
    m_LightIndices = m_Shader->GetUniformVariable("lightIndices");
}

void Pine::Renderer3D::UseRenderingContext(RenderingContext *renderingContext)
{
    m_RenderingContext = renderingContext;
}

void Pine::Renderer3D::SetCamera(Camera* camera)
{
    m_Camera = camera;

    ShaderStorages::Matrix.Data().Projection = camera->GetProjectionMatrix();
    ShaderStorages::Matrix.Data().View = camera->GetViewMatrix();

    ShaderStorages::Matrix.Upload();
}

Pine::Camera *Pine::Renderer3D::GetCamera()
{
    return m_Camera;
}

void Pine::Renderer3D::AddLight(const Light *light)
{
    if (m_CurrentLightIndex >= Specifications::General::DYNAMIC_LIGHT_COUNT)
    {
        Log::Warning("Maximum number of dynamic lights reached.");

        return;
    }

    const int lightSlot = light->GetLightType() == LightType::Directional ? 0 : m_CurrentLightIndex++;
    auto& lightData = ShaderStorages::Lights.Data().Lights[lightSlot];

    const auto rotation = normalize(rotate(light->GetParent()->GetTransform()->GetRotation(), Vector3f(0.f, 0.f, -1.f)));

    lightData.Position = light->GetParent()->GetTransform()->GetPosition();
    lightData.Rotation = rotation;
    lightData.Color = light->GetLightColor();
    lightData.Attenuation = light->GetLightAttenuation();
    lightData.Angle = light->GetSpotlightRadius();
    lightData.AngleSmoothness = light->GetSpotlightCutoff();

    if (light->GetLightType() == LightType::SpotLight)
    {
        if (m_LightIndicesData.x == 0)
            m_LightIndicesData.x = lightSlot;
        else
            m_LightIndicesData.y = lightSlot;
    }

    if (light->GetLightType() == LightType::PointLight)
    {
        if (m_LightIndicesData.z == 0)
            m_LightIndicesData.z = lightSlot;
        else
            m_LightIndicesData.w = lightSlot;
    }
}

void Pine::Renderer3D::UploadLights()
{
    ShaderStorages::Lights.Upload();

    m_CurrentLightIndex = 1;
}

void Pine::Renderer3D::FrameReset()
{
    for (auto& Light : ShaderStorages::Lights.Data().Lights)
    {
        Light.Color = Vector3f(0.0f, 0.0f, 0.0f);
    }

    m_RenderingContext = nullptr;
    m_Shader = nullptr;
    m_Mesh = nullptr;
    m_Material = nullptr;
    m_LightIndicesData = Vector4i(0);
}
