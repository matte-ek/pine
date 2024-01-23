#include "Material.hpp"

#include "Pine/Core/Serialization/Serialization.hpp"

Pine::Material::Material()
{
	m_Type = AssetType::Material;
    m_Shader = Assets::Get<Pine::Shader>("engine/shaders/3d/generic.shader");
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

void Pine::Material::SetDiffuse(const std::string &fileReference)
{
    assert(!fileReference.empty());
    assert(Pine::Assets::GetState() == AssetManagerState::LoadDirectory);

    Assets::AddAssetResolveReference({fileReference, reinterpret_cast<AssetHandle<IAsset>*>(&m_Diffuse)});
}

void Pine::Material::SetSpecular(const std::string &fileReference)
{
    assert(!fileReference.empty());
    assert(Pine::Assets::GetState() == AssetManagerState::LoadDirectory);

    Assets::AddAssetResolveReference({fileReference, reinterpret_cast<AssetHandle<IAsset>*>(&m_Specular)});
}

void Pine::Material::SetNormal(const std::string &fileReference)
{
    assert(!fileReference.empty());
    assert(Pine::Assets::GetState() == AssetManagerState::LoadDirectory);

    Assets::AddAssetResolveReference({fileReference, reinterpret_cast<AssetHandle<IAsset>*>(&m_Normal)});
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

bool Pine::Material::LoadFromFile(AssetLoadStage stage)
{
	const auto json = Serialization::LoadFromFile(m_FilePath);

	if (!json.has_value())
	{
		return false;
	}

	const auto& j = json.value();

	Serialization::LoadVector3(j, "diffuseColor", m_DiffuseColor);
	Serialization::LoadVector3(j, "specularColor", m_SpecularColor);
	Serialization::LoadVector3(j, "ambientColor", m_AmbientColor);

	Serialization::LoadAsset(j, "diffuse", m_Diffuse);
	Serialization::LoadAsset(j, "specular", m_Specular);
	Serialization::LoadAsset(j, "normal", m_Normal);

	Serialization::LoadAsset(j, "shader", m_Shader);

	Serialization::LoadValue(j, "renderingMode", m_RenderingMode);

	Serialization::LoadValue(j, "shininess", m_Shininess);
	Serialization::LoadValue(j, "textureScale", m_TextureScale);

	m_State = AssetState::Loaded;

	return true;
}

bool Pine::Material::SaveToFile()
{
	nlohmann::json j;

	j["diffuseColor"] = Serialization::StoreVector3(m_DiffuseColor);
	j["specularColor"] = Serialization::StoreVector3(m_SpecularColor);
	j["ambientColor"] = Serialization::StoreVector3(m_AmbientColor);

	j["diffuse"] = Serialization::StoreAsset(m_Diffuse.Get());
	j["specular"] = Serialization::StoreAsset(m_Specular.Get());
	j["normal"] = Serialization::StoreAsset(m_Normal.Get());

	j["shader"] = Serialization::StoreAsset(m_Shader.Get());

	j["renderingMode"] = m_RenderingMode;

	j["shininess"] = m_Shininess;
	j["textureScale"] = m_TextureScale;

	Serialization::SaveToFile(m_FilePath, j);

	return true;
}

void Pine::Material::Dispose()
{
}
