#pragma once
#include <glm/vec3.hpp>

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"

namespace Pine
{

	enum class MaterialRenderingMode
	{
		Opaque, // For non transparent textures
		Discard, // For either non-transparent or fully transparent textures
		Transparent, // For semi transparent textures,
		Size
	};

	class Material final : public Asset
	{
	private:
		Vector3f m_DiffuseColor = Vector3f(1.f, 1.f, 1.f);
		Vector3f m_SpecularColor = Vector3f(0.f, 0.f, 0.f);
		Vector3f m_AmbientColor = Vector3f(0.00f, 0.00f, 0.00f);

		AssetHandle<Texture2D> m_Diffuse;
		AssetHandle<Texture2D> m_Specular;
		AssetHandle<Texture2D> m_Normal;

		AssetHandle<Shader> m_Shader;

		MaterialRenderingMode m_RenderingMode = MaterialRenderingMode::Opaque;

		float m_Shininess = 16.f;
		float m_TextureScale = 1.f;

        bool m_IsMeshGeneratedMaterial = true;
	public:
		explicit Material();

		void SetDiffuseColor(Vector3f color);
		void SetSpecularColor(Vector3f color);
		void SetAmbientColor(Vector3f color);

		const Vector3f& GetDiffuseColor() const;
		const Vector3f& GetSpecularColor() const;
		const Vector3f& GetAmbientColor() const;

		void SetDiffuse(Texture2D* texture);
		void SetSpecular(Texture2D* texture);
		void SetNormal(Texture2D* texture);

        void SetDiffuse(const std::string& fileReference);
        void SetSpecular(const std::string& fileReference);
        void SetNormal(const std::string& fileReference);

		Texture2D* GetDiffuse() const;
		Texture2D* GetSpecular() const;
		Texture2D* GetNormal() const;

		void SetShader(Shader* shader);
		Shader* GetShader() const;

		void SetRenderingMode(MaterialRenderingMode mode);
		MaterialRenderingMode GetRenderingMode() const;

		void SetShininess(float value);
		float GetShininess() const;

		void SetTextureScale(float value);
		float GetTextureScale() const;

        // If this material is from a model file, instead of an engine material.
        bool IsMeshGenerated() const;

		bool LoadFromFile(AssetLoadStage stage) override;
		bool SaveToFile() override;

		void Dispose() override;
	};

}
