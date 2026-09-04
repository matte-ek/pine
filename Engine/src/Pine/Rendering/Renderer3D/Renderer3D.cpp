#include "Renderer3D.hpp"

#include "../RenderingContext.hpp"
#include "Specifications.hpp"
#include "ShaderStorages.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

#include <algorithm>
#include <cmath>

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
    ShaderVersion m_ShaderVersion = 0;

    Graphics::IUniformVariable* m_HasTangentData = nullptr;

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
    m_DefaultTexture->UploadTextureData(1, 1, 0, Graphics::TextureFormat::RGBA, Graphics::TextureDataFormat::UnsignedByte, textureData);

    free(textureData);

    ShaderStorages::Matrix.Create();
    ShaderStorages::Instance.Create();
    ShaderStorages::Material.Create();
    ShaderStorages::Lights.Create();
    ShaderStorages::ShadowViews.Create();
    ShaderStorages::World.Create();
}

void Renderer3D::Shutdown()
{
    ShaderStorages::Matrix.Dispose();
    ShaderStorages::Instance.Dispose();
    ShaderStorages::Material.Dispose();
    ShaderStorages::Lights.Dispose();
    ShaderStorages::ShadowViews.Dispose();
    ShaderStorages::World.Dispose();
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

    auto version = Specifications::ShaderVersions::Generic::Default;

    if (m_Material->GetRenderingMode() == MaterialRenderingMode::Discard)
    {
        version = Specifications::ShaderVersions::Generic::Discard;
    }

    const auto shader = m_RenderingConfiguration.OverrideShader ? m_RenderingConfiguration.OverrideShader : m_Material->GetShader();

    if (!shader)
    {
        return;
    }

    if (!shader->HasShaderVersion(0))
    {
        return;
    }

    if (shader->GetProgram() != m_Shader ||
        m_ShaderVersion != static_cast<std::uint32_t>(version) ||
        !shader->IsRendererReady())
    {
        SetShader(shader, static_cast<std::uint32_t>(version));
    }

    if (!m_Shader)
    {
        return;
    }

    // Every shadow in the engine comes out of this one texture - cascades included, since they
    // were folded in. There is no "has a shadow map" uniform to go with it: a light that is not
    // casting carries shadowViewIndex -1, so the shader never reaches the sampler at all.
    if (auto* shadowAtlas = Rendering::ShadowAtlas::GetTexture())
    {
        shadowAtlas->Bind(Specifications::Samplers::SHADOW_ATLAS);
    }

    // Apply Textures

    // Diffuse
    if (m_Material->GetDiffuse())
        m_Material->GetDiffuse()->GetGraphicsTexture()->Bind(Specifications::Samplers::BASE_DIFFUSE);
    else
        m_DefaultTexture->Bind(Specifications::Samplers::BASE_DIFFUSE);

    // Specular
    if (m_Material->GetSpecular())
        m_Material->GetSpecular()->GetGraphicsTexture()->Bind(Specifications::Samplers::BASE_SPECULAR);
    else
        m_DefaultTexture->Bind(Specifications::Samplers::BASE_SPECULAR);

    // Normal
    if (m_Material->GetNormal())
        m_Material->GetNormal()->GetGraphicsTexture()->Bind(Specifications::Samplers::BASE_NORMAL);
    else
        m_DefaultTexture->Bind(Specifications::Samplers::BASE_NORMAL);

    /* Material Properties */
    auto& materialData = ShaderStorages::Material.Data().Properties[0];

    // Authored colors are sRGB; decode to linear here so the shader receives linear data.
    materialData.DiffuseColor = SrgbToLinear(m_Material->GetDiffuseColor());
    materialData.SpecularColor = SrgbToLinear(m_Material->GetSpecularColor());
    materialData.AmbientColor = SrgbToLinear(m_Material->GetAmbientColor());
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

        for (int i = 0; i < Specifications::ObjectLightSlots::COUNT;i++)
        {
            if (auto light = data->LightSlotIndex[i].Get())
            {
                lightIndices[i] = light->GetLightHintData().LightIndex;
            }
            else
            {
                lightIndices[i] = 0;
            }
        }
    }

    return isFull;
}

void Renderer3D::RenderMesh(const Matrix4f& transformationMatrix, ModelRendererHintData* data, const int writeStencilBuffer)
{
    if (data != nullptr)
    {
        auto& lightIndices = ShaderStorages::Instance.Data().Instances[0].LightIndices;

        for (int i = 0; i < Specifications::ObjectLightSlots::COUNT;i++)
        {
            if (auto light = data->LightSlotIndex[i].Get())
            {
                lightIndices[i] = light->GetLightHintData().LightIndex;
            }
            else
            {
                lightIndices[i] = 0;
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
        m_RenderingContext->Statistics.DrawCalls++;
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
        m_RenderingContext->Statistics.DrawCalls++;
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
            if (!shader->CompileShaderVersion(preferredVersion))
            {
                PWarning("Unable to compile requested shader version, rendering may be affected..");
                version = static_cast<std::uint32_t>(Specifications::ShaderVersions::Generic::Default);
            }
        }
    }
    else
    {
        version = static_cast<std::uint32_t>(Specifications::ShaderVersions::Generic::Default);
    }

    const auto shaderProgram = shader->GetProgram(version);

    // Make sure the renderer's shader storages has been set up properly.
    // TODO: Future vision, this "is ready" crap should probably be handled by the renderer or something instead.
    if (!shader->IsRendererReady(version))
    {
        if (!ShaderStorages::Matrix.AttachShaderProgram(shaderProgram))
        {
            PWarning("Renderer3D: Shader is missing 'Matrix' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Instance.AttachShaderProgram(shaderProgram))
        {
            PWarning("Renderer3D: Shader is missing 'Transform' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Material.AttachShaderProgram(shaderProgram))
        {
            PWarning("Renderer3D: Shader is missing 'Material' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::Lights.AttachShaderProgram(shaderProgram))
        {
            PWarning("Renderer3D: Shader is missing 'Lights' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::ShadowViews.AttachShaderProgram(shaderProgram))
        {
            PWarning("Renderer3D: Shader is missing 'ShadowViews' shader storage, expect rendering issues.");
        }

        if (!ShaderStorages::World.AttachShaderProgram(shaderProgram))
        {
            PWarning("Renderer3D: Shader is missing 'World' shader storage, expect rendering issues.");
        }

        shader->SetRendererReady(true, version);
    }

    m_Shader = shaderProgram;
    m_Shader->Use();

    m_ShaderVersion = version;

    m_HasTangentData = m_Shader->GetUniformVariable("hasTangentData");
}

void Renderer3D::PrepareScene(const Vector3f ambientColor, const Vector4f fogColor, const float fogDistance, const float fogIntensity)
{
    auto& worldData = ShaderStorages::World.Data();

    // Authored colors are sRGB; decode to linear here so the shader receives linear data.
    worldData.AmbientColor = Vector4f(SrgbToLinear(ambientColor), 1.f);
    worldData.FogColor = SrgbToLinear(fogColor);
    worldData.FogSettings  = Vector4f(fogDistance, fogIntensity, 0, 0);

    ShaderStorages::World.Upload();
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

void Renderer3D::SetViewProjection(const Matrix4f& viewProjection)
{
    SetCamera(Matrix4f(1.f), viewProjection);
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
        PWarning("Maximum number of dynamic lights reached.");

        return;
    }

    // Negated so the uploaded vector points *towards* the light rather than along its forward
    // axis - that is the convention the whole lighting path uses (see the Light struct in
    // data/engine/shaders/3d/shared/common.glsl).
    const auto directionToLight = -normalize(rotate(light->GetParent()->GetTransform()->GetRotation(), Vector3f(0.f, 0.f, -1.f)));
    const int lightSlot = light->GetLightType() == LightType::Directional ? 0 : m_CurrentLightIndex++;
    auto& lightData = ShaderStorages::Lights.Data().Lights[lightSlot];

    // Cone half-angles are authored in degrees; the shader wants cosines to compare against a dot
    // product. cos() is decreasing, so the *outer* (wider) angle yields the *smaller* cosine, which
    // is what smoothstep needs as edge0. The epsilon keeps the two edges apart: equal edges make
    // smoothstep divide by zero, which is undefined in GLSL.
    //
    // Separate them by pushing the outer edge *down* rather than the inner edge up. Nudging the
    // inner edge up can carry it past 1.0, which no dot product can reach - a very tight cone would
    // then never reach full brightness instead of just being narrow.
    constexpr float minimumConeEdgeSeparation = 0.001f;

    const float cutOffInner = std::cos(glm::radians(light->GetSpotlightInnerAngle()));
    const float cutOffOuter = std::min(std::cos(glm::radians(light->GetSpotlightOuterAngle())),
                                       cutOffInner - minimumConeEdgeSeparation);

    lightData.Position = light->GetParent()->GetTransform()->GetPosition();
    lightData.DirectionToLight = directionToLight;
    lightData.Color = SrgbToLinear(light->GetLightColor()) * light->GetLightIntensity();
    lightData.Range = light->GetRange();

    // Allocated at scene level in Pipeline3D::Prepare, consumed here per rendering context. -1
    // when this light is not casting, which the shader treats as fully lit.
    lightData.ShadowViewIndex = light->GetLightHintData().ShadowViewIndex;
    lightData.ShadowViewCount = light->GetLightHintData().ShadowViewCount;
    lightData.ShadowFade = lightData.ShadowViewIndex >= 0 ? 1.f : 0.f;
    lightData.CutOffOuter = cutOffOuter;
    lightData.CutOffInner = cutOffInner;

    light->GetLightHintData().LightIndex = lightSlot;

    auto& [_, LightIndices] = ShaderStorages::Instance.Data().Instances[0];

    // Fills the first free slot of the light's class. Both branches scan rather than write a fixed
    // index: the spot branch used to assign SPOT_LIGHT_OFFSET directly, which was correct only for
    // as long as there was exactly one spot slot and would have silently kept handling one after
    // SPOT_LIGHT_COUNT was raised.
    const auto claimSlot = [&LightIndices, lightSlot](const int offset, const int count)
    {
        for (int i = 0; i < count; i++)
        {
            if (LightIndices[offset + i] == 0)
            {
                LightIndices[offset + i] = lightSlot;

                return;
            }
        }
    };

    if (light->GetLightType() == LightType::PointLight)
    {
        claimSlot(Specifications::ObjectLightSlots::POINT_LIGHT_OFFSET,
                  Specifications::ObjectLightSlots::POINT_LIGHT_COUNT);
    }
    else if (light->GetLightType() == LightType::SpotLight)
    {
        claimSlot(Specifications::ObjectLightSlots::SPOT_LIGHT_OFFSET,
                  Specifications::ObjectLightSlots::SPOT_LIGHT_COUNT);
    }
}

void Renderer3D::UploadLights()
{
    ShaderStorages::Lights.Upload();

    m_CurrentLightIndex = 1;
}

void Renderer3D::FrameReset()
{
    for (auto& Light : ShaderStorages::Lights.Data().Lights)
    {
        Light.Color = Vector3f(0.0f, 0.0f, 0.0f);

        // Cleared, not just the colour. lights[0] is the directional slot whether or not the level
        // has a sun, and the shader now decides "is there a directional shadow" from this field
        // alone - a stale index from a level that did have one would sample a cascade nothing wrote.
        Light.ShadowViewIndex = -1;
        Light.ShadowViewCount = 0;
    }

    auto& [_, LightIndices] = ShaderStorages::Instance.Data().Instances[0];
    for (int i = 0; i < 8;i++)
    {
        LightIndices[i] = 0;
    }

    ShaderStorages::World.Data().AmbientColor = Vector4f(0.f, 0.f, 0.f, 1.f);
    ShaderStorages::World.Data().FogColor = Vector4f(0.f, 0.f, 0.f, 0.f);
    ShaderStorages::World.Data().FogSettings = Vector4f(25.f, 0.f, 0.f, 0.f);
    ShaderStorages::World.Upload();

    m_RenderingContext = nullptr;
    m_Shader = nullptr;
    m_Mesh = nullptr;
    m_Material = nullptr;

    m_RenderingConfiguration.OverrideShader = nullptr;
    m_RenderingConfiguration.OverrideMaterial = nullptr;
    m_RenderingConfiguration.IgnoreShaderVersions = false;
    m_RenderingConfiguration.SkipMaterialInitialization = false;
}
