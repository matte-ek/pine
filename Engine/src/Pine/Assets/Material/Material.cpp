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
    materialSerializer.Shininess.Read(m_Shininess);
    materialSerializer.TextureScale.Read(m_TextureScale);

    m_IsMeshGeneratedMaterial = false;

    return true;
}

Pine::Material::Material()
{
    m_Type = AssetType::Material;
    m_Shader = Assets::Get<Shader>("engine/shaders/3d/generic.shader");
}

void Pine::Material::SetDiffuseColor(Vector3f color)
{
    m_DiffuseColor = color;
}

void Pine::Material::SetSpecularColor(Vector3f color)
{
    m_SpecularColor = color;
}

void Pine::Material::SetAmbientColor(Vector3f color)
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

void Pine::Material::SetRenderingMode(MaterialRenderingMode mode)
{
    m_RenderingMode = mode;
}

Pine::MaterialRenderingMode Pine::Material::GetRenderingMode() const
{
    return m_RenderingMode;
}

void Pine::Material::SetShininess(float value)
{
    m_Shininess = value;
}

float Pine::Material::GetShininess() const
{
    return m_Shininess;
}

void Pine::Material::SetTextureScale(float value)
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
    materialSerializer.Shininess.Write(m_Shininess);
    materialSerializer.TextureScale.Write(m_TextureScale);

    return materialSerializer.Write();
}

void Pine::Material::Dispose()
{
}
