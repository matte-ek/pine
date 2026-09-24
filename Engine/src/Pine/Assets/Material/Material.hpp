#pragma once
#include <glm/vec3.hpp>

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"

namespace Pine
{
	class Model;

	enum class MaterialRenderingMode
	{
		Opaque, // For non transparent textures
		Discard, // For either non-transparent or fully transparent textures
		Transparent, // For semi transparent textures,
		Size
	};

	// Which of a surface's two faces the rasterizer keeps.
	//
	// 'Default' leaves that to whichever pass is drawing, which is the only honest name for it: the
	// scene pass culls back faces, and a shadow cascade culls front faces to buy its depth
	// separation for free. 'Both' turns culling off for this material's geometry in every pass.
	//
	// 'Both' is for geometry that is a surface rather than a solid - a leaf card, a sheet of grass,
	// a curtain - where there is no interior for the cull to hide and the far side is something the
	// camera is meant to see. A face kept this way is lit with its normal flipped towards the
	// viewer (see generic.fragment.glsl), or the back of every leaf would shade as if it faced the
	// other way.
	//
	// Per-face values (keep only the front, keep only the back) would go here if something needs
	// them. Nothing today wants to override *which* single face is kept, only whether one is.
	enum class MaterialRenderFace
	{
		Default,
		Both
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

	    // Generic 3D shader used as default.
		AssetHandle<Shader> m_Shader = UId("271a649316d8-3cdeb0026f7b7317");

		MaterialRenderingMode m_RenderingMode = MaterialRenderingMode::Opaque;

		MaterialRenderFace m_RenderFace = MaterialRenderFace::Default;

		// Scales the surface's opacity, on top of whatever alpha the diffuse texture carries.
		// Only the Transparent rendering mode reads it: the opaque and discard passes render
		// with blending off, so their output alpha stays 1 whatever is set here.
		float m_Alpha = 1.f;

		float m_Shininess = 16.f;
		float m_TextureScale = 1.f;

		// The model this material is embedded in, or empty for a material with a '.passet' of its
		// own. An embedded material is stored inside that model's payload, so it has no file to
		// save to: it is saved by saving the model.
		UId m_EmbeddingModel;

	    struct MaterialSerializer : Serialization::Serializer
	    {
	        PINE_SERIALIZE_PRIMITIVE(DiffuseColor, Serialization::DataType::Vec3);
	        PINE_SERIALIZE_PRIMITIVE(SpecularColor, Serialization::DataType::Vec3);
	        PINE_SERIALIZE_PRIMITIVE(AmbientColor, Serialization::DataType::Vec3);

	        PINE_SERIALIZE_ASSET(Diffuse);
	        PINE_SERIALIZE_ASSET(Specular);
	        PINE_SERIALIZE_ASSET(Normal);
	        PINE_SERIALIZE_ASSET(Shader);

	        PINE_SERIALIZE_PRIMITIVE(RenderingMode, Serialization::DataType::Int32);
	        PINE_SERIALIZE_PRIMITIVE(RenderFace, Serialization::DataType::Int32);
	        PINE_SERIALIZE_PRIMITIVE(Alpha, Serialization::DataType::Float32);
	        PINE_SERIALIZE_PRIMITIVE(Shininess, Serialization::DataType::Float32);
	        PINE_SERIALIZE_PRIMITIVE(TextureScale, Serialization::DataType::Float32);
	    };

	    bool LoadAssetData(const ByteSpan& span) override;
	    ByteSpan SaveAssetData() override;
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

		Texture2D* GetDiffuse() const;
		Texture2D* GetSpecular() const;
		Texture2D* GetNormal() const;

		void SetShader(Shader* shader);
		Shader* GetShader() const;

		void SetRenderingMode(MaterialRenderingMode mode);
		MaterialRenderingMode GetRenderingMode() const;

		void SetRenderFace(MaterialRenderFace face);
		MaterialRenderFace GetRenderFace() const;

		// Picks the rendering mode that matches what the diffuse texture does with its alpha: a
		// solid texture belongs in the opaque pass, a cutout mask in the discard pass, and one that
		// actually fades in the blended one. Meant to be called wherever a material is given a new
		// diffuse map - the model importer and the editor both do - so that importing content with
		// alpha textures doesn't leave every material to be fixed by hand.
		//
		// Does nothing when there is no diffuse map, or when the texture predates alpha detection
		// and has never been re-imported: the material then keeps the mode it already had.
		void ResolveRenderingModeFromDiffuse();

		void SetAlpha(float value);
		float GetAlpha() const;

		void SetShininess(float value);
		float GetShininess() const;

		void SetTextureScale(float value);
		float GetTextureScale() const;

		// Whether this material came from a model file and is stored inside that model's '.passet'.
		bool IsEmbedded() const;

		// The model this material is embedded in, or nullptr for a standalone material.
		Model* GetEmbeddingModel() const;

		void Dispose() override;

		friend class Model;
	};

}
