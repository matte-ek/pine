#include "Renderer3D.hpp"

#include "Specifications.hpp"
#include "ShaderStorages.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    Pine::Renderer3D::RenderConfiguration m_RenderingConfiguration;

    // Cached graphics API for the current context
    Pine::Graphics::IGraphicsAPI* m_GraphicsAPI = nullptr;

    // This default texture is a solid white pixel, that we treat as a "no-texture" texture.
    Pine::Graphics::ITexture* m_DefaultTexture = nullptr;

    Pine::Graphics::IShaderProgram* m_Shader = nullptr;
    Pine::Graphics::IUniformVariable* m_HasTangentData = nullptr;

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

    m_Material = m_RenderingConfiguration.OverrideMaterial ? m_RenderingConfiguration.OverrideMaterial : overrideMaterial ? overrideMaterial : mesh->GetMaterial();

    auto shader = m_RenderingConfiguration.OverrideShader ? m_RenderingConfiguration.OverrideShader : m_Material->GetShader();
    if (shader->GetProgram() != m_Shader ||
        !shader->IsReady())
    {
        SetShader(shader);
    }

    if (!m_Shader)
    {
        return;
    }

    /* Apply Textures */

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

    m_HasTangentData->LoadInteger(m_Material->GetNormal() != nullptr);
}

bool Pine::Renderer3D::AddInstance(const Matrix4f&transformationMatrix)
{
    if (m_CurrentInstanceIndex >= Specifications::General::MAX_INSTANCE_COUNT)
    {
        return true;
    }

    ShaderStorages::Transform.Data().TransformationMatrix[m_CurrentInstanceIndex++] = transformationMatrix;

    return false;
}

void Pine::Renderer3D::RenderMesh(const Matrix4f&transformationMatrix)
{
    ShaderStorages::Transform.Data().TransformationMatrix[0] = transformationMatrix;
    ShaderStorages::Transform.Upload(sizeof(Matrix4f));

    if (m_Mesh->HasElementBuffer())
    {
        m_GraphicsAPI->DrawElements(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount());
    }
    else
    {
        m_GraphicsAPI->DrawArrays(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount());
    }
}

void Pine::Renderer3D::RenderMeshInstanced()
{
    if (m_CurrentInstanceIndex == 0)
    {
        return;
    }

    ShaderStorages::Transform.Upload(sizeof(Matrix4f) * m_CurrentInstanceIndex);

    if (m_Mesh->HasElementBuffer())
    {
        m_GraphicsAPI->DrawElementsInstanced(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount(), m_CurrentInstanceIndex);
    }
    else
    {
        m_GraphicsAPI->DrawArraysInstanced(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount(), m_CurrentInstanceIndex);
    }

    m_CurrentInstanceIndex = 0;
}

void Pine::Renderer3D::SetShader(Shader*shader)
{
    if (!shader->IsReady())
    {
        if (!ShaderStorages::Matrix.AttachShader(shader))
        {
            Log::Error("Shader is missing 'Matrix' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Transform.AttachShader(shader))
        {
            Log::Error("Shader is missing 'Transform' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Material.AttachShader(shader))
        {
            Log::Error("Shader is missing 'Material' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Lights.AttachShader(shader))
        {
            Log::Error("Shader is missing 'Lights' shader storage, expect rendering issues.");
        }

        shader->SetReady(true);
    }

    m_HasTangentData = shader->GetProgram()->GetUniformVariable("hasTangentData");
    m_Shader = shader->GetProgram();

    m_Shader->Use();

    assert(m_HasTangentData);
}

void Pine::Renderer3D::SetCamera(Camera*camera)
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

    lightData.Position = light->GetParent()->GetTransform()->GetPosition();
    lightData.Rotation = light->GetParent()->GetTransform()->GetEulerAngles(); // TODO: This won't account for parent rotations
    lightData.Color = light->GetLightColor();
}

void Pine::Renderer3D::UploadLights()
{
    ShaderStorages::Lights.Upload();

    m_CurrentLightIndex = 0;
}

void Pine::Renderer3D::FrameReset()
{
    m_Shader = nullptr;
    m_Mesh = nullptr;
    m_Material = nullptr;
}
