#include "Material.hpp"

#include "../../Core/Serialization/Json/SerializationJson.hpp"

bool Pine::Material::LoadAssetData(const ByteSpan& span)
{
    MaterialSerializer materialSerializer;

    if (!materialSerializer.Read(span))
    {
        return false;
    }

    materialSerializer.DiffuseColor.Read(m_DiffuseColor);
    materialSerializer.SpecularColor.Read(m_SpecularColor);
    materialSerializer.AmbientColor.Read(m_AmbientColor);

    materialSerializer.Diffuse.Read(m_Diffuse);
    materialSerializer.Specular.Read(m_Specular);
    materialSerializer.Normal.Read(m_Normal);
    materialSerializer.Shader.Read(m_Shader);

    materialSerializer.RenderingMode.Read(m_RenderingMode);
    materialSerializer.Alpha.Read(m_Alpha);
    materialSerializer.Shininess.Read(m_Shininess);
    materialSerializer.TextureScale.Read(m_TextureScale);

    m_IsMeshGeneratedMaterial = false;

    return true;
}

Pine::Material::Material()
{
    m_Type = AssetType::Material;
}

void Pine::Material::SetDiffuseColor(const Vector3f color)
{
    m_DiffuseColor = color;
}

void Pine::Material::SetSpecularColor(const Vector3f color)
{
    m_SpecularColor = color;
}

void Pine::Material::SetAmbientColor(const Vector3f color)
{
    m_AmbientColor = color;
}

const Pine::Vector3f& Pine::Material::GetDiffuseColor() const
{
    return m_DiffuseColor;
}

const Pine::Vector3f& Pine::Material::GetSpecularColor() const
{
    return m_SpecularColor;
}

const Pine::Vector3f& Pine::Material::GetAmbientColor() const
{
    return m_AmbientColor;
}

void Pine::Material::SetDiffuse(Texture2D* texture)
{
    m_Diffuse = texture;
}

void Pine::Material::SetSpecular(Texture2D* texture)
{
    m_Specular = texture;
}

void Pine::Material::SetNormal(Texture2D* texture)
{
    m_Normal = texture;
}

Pine::Texture2D* Pine::Material::GetDiffuse() const
{
    return m_Diffuse.Get();
}

Pine::Texture2D* Pine::Material::GetSpecular() const
{
    return m_Specular.Get();
}

Pine::Texture2D* Pine::Material::GetNormal() const
{
    return m_Normal.Get();
}

void Pine::Material::SetShader(Shader* shader)
{
    m_Shader = shader;
}

Pine::Shader* Pine::Material::GetShader() const
{
    return m_Shader.Get();
}

void Pine::Material::SetRenderingMode(const MaterialRenderingMode mode)
{
    m_RenderingMode = mode;
}

Pine::MaterialRenderingMode Pine::Material::GetRenderingMode() const
{
    return m_RenderingMode;
}

void Pine::Material::ResolveRenderingModeFromDiffuse()
{
    const auto diffuse = m_Diffuse.Get();

    if (diffuse == nullptr)
    {
        return;
    }

    switch (diffuse->GetAlphaMode())
    {
        case TextureAlphaMode::Opaque:
            m_RenderingMode = MaterialRenderingMode::Opaque;
            break;
        case TextureAlphaMode::Cutout:
            m_RenderingMode = MaterialRenderingMode::Discard;
            break;
        case TextureAlphaMode::Transparent:
            m_RenderingMode = MaterialRenderingMode::Transparent;
            break;
        case TextureAlphaMode::Unknown:
            break;
    }
}

void Pine::Material::SetAlpha(float value)
{
    if (value < 0.f)
    {
        value = 0.f;
    }

    if (value > 1.f)
    {
        value = 1.f;
    }

    m_Alpha = value;
}

float Pine::Material::GetAlpha() const
{
    return m_Alpha;
}

void Pine::Material::SetShininess(float value)
{
    if (value < 0.01f)
    {
        value = 0.01f;
    }

    m_Shininess = value;
}

float Pine::Material::GetShininess() const
{
    return m_Shininess;
}

void Pine::Material::SetTextureScale(const float value)
{
    m_TextureScale = value;
}

float Pine::Material::GetTextureScale() const
{
    return m_TextureScale;
}

bool Pine::Material::IsMeshGenerated() const
{
    return m_IsMeshGeneratedMaterial;
}

Pine::ByteSpan Pine::Material::SaveAssetData()
{
    MaterialSerializer materialSerializer;

    materialSerializer.DiffuseColor.Write(m_DiffuseColor);
    materialSerializer.SpecularColor.Write(m_SpecularColor);
    materialSerializer.AmbientColor.Write(m_AmbientColor);

    materialSerializer.Diffuse.Write(m_Diffuse);
    materialSerializer.Specular.Write(m_Specular);
    materialSerializer.Normal.Write(m_Normal);
    materialSerializer.Shader.Write(m_Shader);

    materialSerializer.RenderingMode.Write(m_RenderingMode);
    materialSerializer.Alpha.Write(m_Alpha);
    materialSerializer.Shininess.Write(m_Shininess);
    materialSerializer.TextureScale.Write(m_TextureScale);

    return materialSerializer.Write();
}

void Pine::Material::Dispose()
{
}
