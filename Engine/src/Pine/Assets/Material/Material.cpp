#include "Material.hpp"

#include "../../Core/Serialization/Json/SerializationJson.hpp"

Pine::Material::Material()
{
	m_Type = AssetType::Material;
    m_Shader = Assets::Get<Pine::Shader>("engine/shaders/3d/generic.shader");
}

void Pine::Material::SetDiffuseColor(Vector3f color)
{
	m_DiffuseColor = color;
    m_HasBeenModified = true;
}

void Pine::Material::SetSpecularColor(Vector3f color)
{
	m_SpecularColor = color;
    m_HasBeenModified = true;
}

void Pine::Material::SetAmbientColor(Vector3f color)
{
	m_AmbientColor = color;
    m_HasBeenModified = true;
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
    m_HasBeenModified = true;
}

void Pine::Material::SetSpecular(Texture2D* texture)
{
	m_Specular = texture;
    m_HasBeenModified = true;
}

void Pine::Material::SetNormal(Texture2D* texture)
{
	m_Normal = texture;
    m_HasBeenModified = true;
}

void Pine::Material::SetDiffuse(const std::string &fileReference)
{
    assert(!fileReference.empty());
    assert(Pine::Assets::GetState() == AssetManagerState::LoadDirectory);

    Assets::AddAssetResolveReference({fileReference, reinterpret_cast<AssetHandle<Asset>*>(&m_Diffuse), AssetType::Texture2D});
}

void Pine::Material::SetSpecular(const std::string &fileReference)
{
    assert(!fileReference.empty());
    assert(Pine::Assets::GetState() == AssetManagerState::LoadDirectory);

    Assets::AddAssetResolveReference({fileReference, reinterpret_cast<AssetHandle<Asset>*>(&m_Specular), AssetType::Texture2D});
}

void Pine::Material::SetNormal(const std::string &fileReference)
{
    assert(!fileReference.empty());
    assert(Pine::Assets::GetState() == AssetManagerState::LoadDirectory);

    Assets::AddAssetResolveReference({fileReference, reinterpret_cast<AssetHandle<Asset>*>(&m_Normal), AssetType::Texture2D});
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
    m_HasBeenModified = true;
}

Pine::Shader* Pine::Material::GetShader() const
{
	return m_Shader.Get();
}

void Pine::Material::SetRenderingMode(MaterialRenderingMode mode)
{
	m_RenderingMode = mode;
    m_HasBeenModified = true;
}

Pine::MaterialRenderingMode Pine::Material::GetRenderingMode() const
{
	return m_RenderingMode;
}

void Pine::Material::SetShininess(float value)
{
	m_Shininess = value;
    m_HasBeenModified = true;
}

float Pine::Material::GetShininess() const
{
	return m_Shininess;
}

void Pine::Material::SetTextureScale(float value)
{
	m_TextureScale = value;
    m_HasBeenModified = true;
}

float Pine::Material::GetTextureScale() const
{
	return m_TextureScale;
}

bool Pine::Material::LoadFromFile(AssetLoadStage stage)
{
	const auto json = SerializationJson::LoadFromFile(m_FilePath);

	if (!json.has_value())
	{
		return false;
	}

	const auto& j = json.value();

	SerializationJson::LoadVector3(j, "diffuseColor", m_DiffuseColor);
	SerializationJson::LoadVector3(j, "specularColor", m_SpecularColor);
	SerializationJson::LoadVector3(j, "ambientColor", m_AmbientColor);

	SerializationJson::LoadAsset(j, "diffuse", m_Diffuse);
	SerializationJson::LoadAsset(j, "specular", m_Specular);
	SerializationJson::LoadAsset(j, "normal", m_Normal);

	SerializationJson::LoadAsset(j, "shader", m_Shader);

	SerializationJson::LoadValue(j, "renderingMode", m_RenderingMode);

	SerializationJson::LoadValue(j, "shininess", m_Shininess);
	SerializationJson::LoadValue(j, "textureScale", m_TextureScale);

    m_IsMeshGeneratedMaterial = false;
	m_State = AssetState::Loaded;

	return true;
}

bool Pine::Material::SaveToFile()
{
	nlohmann::json j;

	j["diffuseColor"] = SerializationJson::StoreVector3(m_DiffuseColor);
	j["specularColor"] = SerializationJson::StoreVector3(m_SpecularColor);
	j["ambientColor"] = SerializationJson::StoreVector3(m_AmbientColor);

	j["diffuse"] = SerializationJson::StoreAsset(m_Diffuse.Get());
	j["specular"] = SerializationJson::StoreAsset(m_Specular.Get());
	j["normal"] = SerializationJson::StoreAsset(m_Normal.Get());

	j["shader"] = SerializationJson::StoreAsset(m_Shader.Get());

	j["renderingMode"] = m_RenderingMode;

	j["shininess"] = m_Shininess;
	j["textureScale"] = m_TextureScale;

	SerializationJson::SaveToFile(m_FilePath, j);

	return true;
}

void Pine::Material::Dispose()
{
}

bool Pine::Material::IsMeshGenerated() const
{
    return m_IsMeshGeneratedMaterial;
}
