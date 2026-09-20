#include "Renderer3D.hpp"

#include "../RenderingContext.hpp"
#include "Specifications.hpp"
#include "ShaderStorages.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"
#include "Pine/Rendering/Renderer3D/LightSlotData.hpp"
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

    // The "no-normal-map" texture: a single pixel holding the encoded form of (0, 0, 1), which
    // decodes to the surface normal itself and so leaves shading exactly as it would be without a
    // normal map. The white default above would decode to a normal pointing out of the corner of
    // the tangent frame instead.
    //
    // It exists for the terrain path, which blends four normal maps unconditionally rather than
    // branching on whether a material has one.
    Graphics::ITexture* m_DefaultNormalTexture = nullptr;

    // The shader a terrain draws with, kept here because PrepareTerrainChunk is what selects it -
    // the same way PrepareMesh takes a mesh's shader off its material. Terrain has no single
    // material to carry one.
    Shader* m_TerrainShader = nullptr;

    Graphics::IShaderProgram* m_Shader = nullptr;
    ShaderVersion m_ShaderVersion = 0;

    Graphics::IUniformVariable* m_HasTangentData = nullptr;
    Graphics::IUniformVariable* m_SplatTransform = nullptr;
    Graphics::IUniformVariable* m_BrushRing = nullptr;

    Mesh* m_Mesh = nullptr;

    int m_CurrentInstanceIndex = 0;

    Material* m_Material = nullptr;

    Camera* m_Camera = nullptr;

    int m_CurrentLightIndex = 0;

    // Resolves one drawn thing's light slots into the light-buffer indices the shader reads, and
    // writes them onto an instance.
    //
    // A slot whose light is gone resolves to index 0, the same as an empty one: the handle has
    // already invalidated itself by then, so a destroyed light cannot leave a live index behind.
    // Null slots leave the instance's indices alone - see AddInstance.
    void WriteInstanceLightIndices(const int instanceId, Renderer3D::LightSlotData* lightSlots)
    {
        if (lightSlots == nullptr)
        {
            return;
        }

        auto& lightIndices = Renderer3D::ShaderStorages::Instance.Data().Instances[instanceId].LightIndices;

        for (int i = 0; i < Renderer3D::Specifications::ObjectLightSlots::COUNT; i++)
        {
            const auto light = lightSlots->Index[i].Get();

            lightIndices[i] = light != nullptr ? light->GetLightHintData().LightIndex : 0;
        }
    }

    // Binds one terrain layer's textures and writes its properties into the material buffer slot
    // the terrain shader reads that layer from.
    //
    // A layer with no material assigned still gets a full slot rather than being skipped: it can
    // carry weight in the splat map, and an unwritten slot would show it as whatever the previous
    // draw happened to leave there. The values it gets are the ones an untextured Material
    // default-constructs with, so an unassigned channel reads as plain white.
    void BindTerrainLayer(const int layer, Material* material)
    {
        const auto diffuse = material != nullptr ? material->GetDiffuse() : nullptr;
        const auto specular = material != nullptr ? material->GetSpecular() : nullptr;
        const auto normal = material != nullptr ? material->GetNormal() : nullptr;

        (diffuse != nullptr ? diffuse->GetGraphicsTexture() : m_DefaultTexture)
            ->Bind(Renderer3D::Specifications::Samplers::BASE_DIFFUSE + layer);

        (specular != nullptr ? specular->GetGraphicsTexture() : m_DefaultTexture)
            ->Bind(Renderer3D::Specifications::Samplers::BASE_SPECULAR + layer);

        (normal != nullptr ? normal->GetGraphicsTexture() : m_DefaultNormalTexture)
            ->Bind(Renderer3D::Specifications::Samplers::BASE_NORMAL + layer);

        auto& materialData = Renderer3D::ShaderStorages::Material.Data().Properties[layer];

        if (material == nullptr)
        {
            materialData.DiffuseColor = Vector3f(1.f);
            materialData.SpecularColor = Vector3f(0.f);
            materialData.AmbientColor = Vector3f(0.f);
            materialData.Shininess = 16.f;
            materialData.UVScale = 1.f;
            materialData.Alpha = 1.f;

            return;
        }

        // Authored colors are sRGB; decode to linear here so the shader receives linear data.
        materialData.DiffuseColor = SrgbToLinear(material->GetDiffuseColor());
        materialData.SpecularColor = SrgbToLinear(material->GetSpecularColor());
        materialData.AmbientColor = SrgbToLinear(material->GetAmbientColor());
        materialData.Shininess = material->GetShininess();
        materialData.UVScale = material->GetTextureScale();
        materialData.Alpha = material->GetAlpha();
    }
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

    // (0, 0, 1) in the usual tangent-space encoding, so sampling it is the same as having no
    // normal map at all.
    std::uint8_t flatNormal[4] = { 128, 128, 255, 255 };

    m_DefaultNormalTexture = m_GraphicsAPI->CreateTexture();

    m_DefaultNormalTexture->Bind();
    m_DefaultNormalTexture->UploadTextureData(1, 1, 0, Graphics::TextureFormat::RGBA, Graphics::TextureDataFormat::UnsignedByte, flatNormal);

    m_TerrainShader = Assets::Get<Shader>("engine/shaders/3d/terrain");

    ShaderStorages::Matrix.Create();
    ShaderStorages::Instance.Create();
    ShaderStorages::Material.Create();
    ShaderStorages::Lights.Create();
    ShaderStorages::ShadowViews.Create();
    ShaderStorages::World.Create();
}

void Renderer3D::Shutdown()
{
    m_GraphicsAPI->DestroyTexture(m_DefaultTexture);
    m_GraphicsAPI->DestroyTexture(m_DefaultNormalTexture);

    m_DefaultTexture = nullptr;
    m_DefaultNormalTexture = nullptr;
    m_TerrainShader = nullptr;

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

Pine::Material* Renderer3D::ResolveMaterial(Mesh* mesh, Material* overrideMaterial)
{
    if (m_RenderingConfiguration.OverrideMaterial != nullptr)
    {
        return m_RenderingConfiguration.OverrideMaterial;
    }

    if (overrideMaterial != nullptr)
    {
        return overrideMaterial;
    }

    return mesh->GetMaterial();
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

    m_Material = ResolveMaterial(mesh, overrideMaterial);

    if (!m_Material)
    {
        return;
    }

    auto version = Specifications::ShaderVersions::Generic::Default;

    if (m_Material->GetRenderingMode() == MaterialRenderingMode::Discard)
    {
        version = Specifications::ShaderVersions::Generic::Discard;
    }
    else if (m_Material->GetRenderingMode() == MaterialRenderingMode::Transparent)
    {
        version = Specifications::ShaderVersions::Generic::Transparent;
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
    materialData.Alpha = m_Material->GetAlpha();

    ShaderStorages::Material.Upload();

    if (m_HasTangentData)
    {
        m_HasTangentData->LoadInteger(m_Material->GetNormal() != nullptr);
    }
}

// The renderer and the asset each carry their own half of the layer count; they describe the same
// four channels, so a change to one without the other would silently drop or duplicate a layer.
static_assert(Renderer3D::Specifications::TerrainLayers::COUNT == Terrain::MaximumLayerCount,
    "The terrain layer count in Specifications.hpp and on the Terrain asset have drifted apart.");

void Renderer3D::PrepareTerrainChunk(Mesh* mesh,
                                     const std::array<Material*, Specifications::TerrainLayers::COUNT>& layers,
                                     Graphics::ITexture* splatMap,
                                     const Vector4f& splatTransform,
                                     const Vector4f* brushRing)
{
    mesh->GetVertexArray()->Bind();

    m_CurrentInstanceIndex = 0;
    m_Mesh = mesh;

    // The depth pre-pass and the shadow passes draw with their own shader and read nothing off the
    // surface, so there is no blend for them to set up - same early exit PrepareMesh takes.
    if (m_RenderingConfiguration.SkipMaterialInitialization)
    {
        if (m_RenderingConfiguration.OverrideShader)
        {
            SetShader(m_RenderingConfiguration.OverrideShader);
        }

        return;
    }

    const auto shader = m_RenderingConfiguration.OverrideShader ? m_RenderingConfiguration.OverrideShader : m_TerrainShader;

    if (shader == nullptr || !shader->HasShaderVersion(0))
    {
        return;
    }

    // The brush overlay is the terrain shader's only variant, and only the editor ever asks for it.
    // The cached version has to be compared either way, because the last draw may have left a
    // generic shader's variant selected.
    const auto version = static_cast<ShaderVersion>(brushRing != nullptr
        ? Specifications::ShaderVersions::Terrain::Brush
        : Specifications::ShaderVersions::Terrain::Default);

    if (shader->GetProgram(shader->HasShaderVersion(version) ? version : 0) != m_Shader ||
        m_ShaderVersion != version ||
        !shader->IsRendererReady(version))
    {
        SetShader(shader, version);
    }

    if (!m_Shader)
    {
        return;
    }

    // Nothing that follows goes through m_Material, and leaving the previous draw's material in it
    // would describe this one wrongly to anything that reads it later.
    m_Material = nullptr;

    if (auto* shadowAtlas = Rendering::ShadowAtlas::GetTexture())
    {
        shadowAtlas->Bind(Specifications::Samplers::SHADOW_ATLAS);
    }

    for (int layer = 0; layer < Specifications::TerrainLayers::COUNT; layer++)
    {
        BindTerrainLayer(layer, layers[layer]);
    }

    // Null only before the terrain's first Prepare has run, which the caller already skips over.
    // The white default blends every layer evenly rather than leaving an unbound sampler behind.
    if (splatMap != nullptr)
    {
        splatMap->Bind(Specifications::Samplers::SPLAT_MAP);
    }
    else
    {
        m_DefaultTexture->Bind(Specifications::Samplers::SPLAT_MAP);
    }

    ShaderStorages::Material.Upload();

    if (m_SplatTransform != nullptr)
    {
        m_SplatTransform->LoadVector4(splatTransform);
    }

    // Null whenever the plain version is bound, which is every draw that passed no ring.
    if (m_BrushRing != nullptr && brushRing != nullptr)
    {
        m_BrushRing->LoadVector4(*brushRing);
    }
}

bool Renderer3D::AddInstance(const Matrix4f& transformationMatrix, LightSlotData* lightSlots)
{
    const bool isFull = m_CurrentInstanceIndex == Specifications::General::MAX_INSTANCE_COUNT - 1;
    const int instanceId = m_CurrentInstanceIndex++;

    ShaderStorages::Instance.Data().Instances[instanceId].TransformationMatrix = transformationMatrix;

    WriteInstanceLightIndices(instanceId, lightSlots);

    return isFull;
}

void Renderer3D::RenderMesh(const Matrix4f& transformationMatrix,
                            LightSlotData* lightSlots,
                            const int writeStencilBuffer,
                            const std::uint32_t indexCount)
{
    WriteInstanceLightIndices(0, lightSlots);

    ShaderStorages::Instance.Data().Instances[0].TransformationMatrix = transformationMatrix;
    ShaderStorages::Instance.Upload(sizeof(ShaderStorages::InstanceData::Instance));

    if (writeStencilBuffer != 0)
    {
        m_GraphicsAPI->SetStencilOperation(Graphics::StencilOperation::Keep, Graphics::StencilOperation::Keep, Graphics::StencilOperation::Replace);
        m_GraphicsAPI->SetStencilFunction(Graphics::TestFunction::Always, writeStencilBuffer, 0x0);
    }

    // Clamped rather than trusted, so a caller asking for more than the mesh holds draws the mesh
    // instead of reading past the end of its index buffer.
    const auto drawCount = indexCount == 0
        ? m_Mesh->GetRenderCount()
        : std::min(indexCount, m_Mesh->GetRenderCount());

    if (m_Mesh->HasElementBuffer())
    {
        m_GraphicsAPI->DrawElements(Graphics::RenderMode::Triangles, drawCount);
    }
    else
    {
        m_GraphicsAPI->DrawArrays(Graphics::RenderMode::Triangles, drawCount);
    }

    if (m_RenderingContext != nullptr)
    {
        m_RenderingContext->Statistics.DrawCalls++;
        m_RenderingContext->Statistics.VertexCount += drawCount;
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
        m_RenderingContext->Statistics.VertexCount +=
            static_cast<std::uint64_t>(m_Mesh->GetRenderCount()) * m_CurrentInstanceIndex;
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

    // Asked for only where it exists. A lookup that misses warns and then caches the miss, so
    // asking every shader would put one warning per program in the log at startup and say nothing.
    m_SplatTransform = shader == m_TerrainShader ? m_Shader->GetUniformVariable("splatTransform") : nullptr;

    // Only exists in the terrain shader's brush version; every other program would report a miss,
    // cache it, and leave one warning per program in the log.
    m_BrushRing = shader == m_TerrainShader && version == static_cast<ShaderVersion>(Specifications::ShaderVersions::Terrain::Brush)
        ? m_Shader->GetUniformVariable("brushRing")
        : nullptr;
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
    lightData.CutOffOuter = cutOffOuter;
    lightData.CutOffInner = cutOffInner;

    light->GetLightHintData().LightIndex = lightSlot;

    auto& lightIndices = ShaderStorages::Instance.Data().Instances[0].LightIndices;

    // Fills the first free slot of the light's class. Both branches scan rather than write a fixed
    // index: the spot branch used to assign SPOT_LIGHT_OFFSET directly, which was correct only for
    // as long as there was exactly one spot slot and would have silently kept handling one after
    // SPOT_LIGHT_COUNT was raised.
    const auto claimSlot = [&lightIndices, lightSlot](const int offset, const int count)
    {
        for (int i = 0; i < count; i++)
        {
            if (lightIndices[offset + i] == 0)
            {
                lightIndices[offset + i] = lightSlot;

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

    auto& lightIndices = ShaderStorages::Instance.Data().Instances[0].LightIndices;
    for (int i = 0; i < 8;i++)
    {
        lightIndices[i] = 0;
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
