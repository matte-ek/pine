#include "Pipeline3D.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Features/Shadows/Shadows.hpp"
#include "Pine/Rendering/Features/Skybox/Skybox.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/World.hpp"

namespace
{
	using namespace Pine;
	using namespace Pine::Pipeline3D;

	Shader* m_DepthShader = nullptr;
	Graphics::IFrameBuffer* m_DepthBuffer = nullptr;

	PipelineConfiguration m_Configuration;

	std::unordered_map<RenderObject, std::uint32_t, RenderObjectHash> m_ModelInstanceCountHint;

	ObjectBatchData m_RenderingBatch;

	// Find and sort all active ModelRenderers in the scene. Will make sure to group together models using the
	// same mesh and material to allow for effective batch rendering. We also make sure to figure out which
	// materials will require discarding and blending.
	void PrepareRenderingBatch()
	{
		m_RenderingBatch = ObjectBatchData();

		for (auto& modelRenderer : Components::Get<ModelRenderer>())
		{
			if (!modelRenderer.GetModel())
			{
				continue;
			}

			// Find out if a mesh within this model has a transparent material
			bool hasTransparentMaterial = false;
			for (const auto& mesh : modelRenderer.GetModel()->GetMeshes())
			{
				if (mesh->GetMaterial() && mesh->GetMaterial()->GetRenderingMode() == MaterialRenderingMode::Transparent)
				{
					hasTransparentMaterial = true;
				}
			}

			const RenderObject uniqueObject = { modelRenderer.GetModel(), modelRenderer.GetOverrideMaterial() };

			// Find out if we have a hint on how many instances this model has, we do this to avoid
			// having to re-allocate the vector too much.
			if (m_RenderingBatch.OpaqueObjects.count(uniqueObject) == 0)
			{
				if (m_ModelInstanceCountHint.count(uniqueObject) != 0)
				{
					m_RenderingBatch.OpaqueObjects[uniqueObject].reserve(m_ModelInstanceCountHint[uniqueObject]);
				}
			}

			m_RenderingBatch.OpaqueObjects[uniqueObject].push_back({&modelRenderer, 0.f});

			if (hasTransparentMaterial)
			{
				m_RenderingBatch.BlendObjects[uniqueObject].push_back({&modelRenderer, 0.f});
			}
		}

		// Store instance count hint for the next frame
		for (const auto&[objectGroup, modelRenderers] : m_RenderingBatch.OpaqueObjects)
		{
			m_ModelInstanceCountHint[objectGroup] = modelRenderers.size();
		}
	}

	void RenderBatch(const ObjectBatchMap& mapBatch, MaterialRenderingMode materialRenderingMode)
	{
		for (const auto& [modelGroup, objectRenderInstances] : mapBatch)
		{
			const auto model = modelGroup.Model;

			int meshIndex = -1;
			for (const auto mesh : model->GetMeshes())
			{
				meshIndex++;

				// Make sure we're rendering materials with the correct mode
				const auto material = modelGroup.OverrideMaterial != nullptr ? modelGroup.OverrideMaterial : mesh->GetMaterial();
				if (material && material->GetRenderingMode() != materialRenderingMode)
				{
					continue;
				}

				Renderer3D::PrepareMesh(mesh, modelGroup.OverrideMaterial);

				bool hasStencilBufferOverride = false;

				for (const auto [renderer, distance] : objectRenderInstances)
				{
					const auto modelRenderer = renderer;

					modelRenderer->GetParent()->GetTransform()->OnRender(0.f);

					if (modelRenderer->GetOverrideStencilBuffer())
					{
						hasStencilBufferOverride = true;
						continue;
					}

					int modelMeshIndex = modelRenderer->GetModelMeshIndex();
					if (modelMeshIndex >= 0)
					{
						if (modelMeshIndex != meshIndex)
						{
							continue;
						}
					}

					if (Renderer3D::AddInstance(modelRenderer->GetParent()->GetTransform()->GetTransformationMatrix()))
					{
						Renderer3D::RenderMeshInstanced();
					}
				}

				Renderer3D::RenderMeshInstanced();

				if (hasStencilBufferOverride)
				{
					for (const auto [renderer, distance] : objectRenderInstances)
					{
						if (renderer->GetOverrideStencilBuffer())
						{
							renderer->GetParent()->GetTransform()->OnRender(0.f);

							Renderer3D::RenderMesh(renderer->GetParent()->GetTransform()->GetTransformationMatrix(), renderer->GetStencilBufferValue());
						}
					}
				}
			}
		}
	}

	std::vector<Light*> GetLights()
	{
		std::vector<Light*> lights;

		for (auto& light : Components::Get<Light>())
		{
			lights.push_back(&light);
		}

		return lights;
	}

	void RenderDepthPrepass(RenderingContext& renderingContext)
	{
		if (m_DepthBuffer == nullptr || renderingContext.SceneCamera == nullptr)
		{
			return;
		}

		auto& renderSettings = Renderer3D::GetRenderConfiguration();

		m_DepthBuffer->Bind();

		Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);
		Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), Vector2i(1920, 1080));
		Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer | Graphics::DepthBuffer);

		Renderer3D::FrameReset();
		Renderer3D::SetCamera(renderingContext.SceneCamera);
		Renderer3D::UseRenderingContext(&renderingContext);

		renderSettings.OverrideShader = m_DepthShader;
		renderSettings.IgnoreShaderVersions = true;
		renderSettings.SkipMaterialInitialization = true;

		RenderBatch(m_RenderingBatch.OpaqueObjects, MaterialRenderingMode::Opaque);

		renderSettings.OverrideShader = nullptr;
		renderSettings.IgnoreShaderVersions = false;
		renderSettings.SkipMaterialInitialization = false;
	}

	void RenderScene(const std::vector<Light*>& lights, RenderingContext& context)
	{
		Renderer3D::FrameReset();

		if (context.SceneCamera)
			Renderer3D::SetCamera(context.SceneCamera);

		Renderer3D::UseRenderingContext(&context);
		Renderer3D::SetAmbientColor(World::GetActiveLevel()->GetLevelSettings().AmbientColor);

		Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);
		Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);

		Graphics::GetGraphicsAPI()->SetBlendingEnabled(true);
		Graphics::GetGraphicsAPI()->SetBlendingFunction(Graphics::BlendingFunction::SourceAlpha, Graphics::BlendingFunction::OneMinusSourceAlpha);

		for (const auto light : lights)
		{
			Renderer3D::AddLight(light);

			if (m_Configuration.RenderShadows)
			{
				Rendering::Shadows::UploadShadowData(light);
			}
		}

		Renderer3D::UploadLights();

		// Render fully opaque objects.
		RenderBatch(m_RenderingBatch.OpaqueObjects, MaterialRenderingMode::Opaque);

		// Render objects which require discarding
		RenderBatch(m_RenderingBatch.OpaqueObjects, MaterialRenderingMode::Discard);

		// TODO: Render semi-transparent objects, we'll have to sort all objects by distance as well.

		// Skybox
		if (context.Skybox != nullptr)
		{
			Rendering::Skybox::Render(context.Skybox);
			context.DrawCalls++;
		}
	}
}

void Pipeline3D::Setup()
{
	Rendering::Skybox::Setup();
	Rendering::Shadows::Setup();
	Rendering::AmbientOcclusion::Setup();

	m_DepthBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
	m_DepthBuffer->Bind();
	m_DepthBuffer->Prepare();

	//m_DepthBuffer->AttachTextures(1920, 1080, Graphics::ColorBuffer | Graphics::DepthBuffer);

	const auto normalBuffer = Graphics::GetGraphicsAPI()->CreateTexture();

	normalBuffer->Bind();
	normalBuffer->UploadTextureData(
		Renderer3D::Specifications::General::INTERNAL_WIDTH,
		Renderer3D::Specifications::General::INTERNAL_HEIGHT,
		Graphics::TextureFormat::RGBA16F,
		Graphics::TextureDataFormat::Float,
		nullptr);

	m_DepthBuffer->AttachTexture(normalBuffer, Graphics::BufferAttachment::Color);

	const auto depthBuffer = Graphics::GetGraphicsAPI()->CreateTexture();

	depthBuffer->Bind();
	depthBuffer->UploadTextureData(
		Renderer3D::Specifications::General::INTERNAL_WIDTH,
		Renderer3D::Specifications::General::INTERNAL_HEIGHT,
		Graphics::TextureFormat::Depth, Graphics::TextureDataFormat::Float,
		nullptr);

	m_DepthBuffer->AttachTexture(depthBuffer, Graphics::BufferAttachment::Depth);

	m_DepthBuffer->Finish();

	Rendering::AmbientOcclusion::UseDepthBuffer(m_DepthBuffer);

	m_DepthShader = Assets::Get<Shader>("engine/shaders/3d/depth.shader");
}

void Pipeline3D::Shutdown()
{
	Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_DepthBuffer);

	Rendering::AmbientOcclusion::Shutdown();
	Rendering::Skybox::Shutdown();
	Rendering::Shadows::Shutdown();
}

void Pipeline3D::Prepare()
{
	PrepareRenderingBatch();
}

void Pipeline3D::Run(RenderingContext& context, PipelineStage stage)
{
	// Prepare Data
	const auto lights = GetLights();

	if (stage == PipelineStage::Prepass)
	{
		// Render shadow pass
		if (m_Configuration.RenderShadows)
		{
			Rendering::Shadows::NewFrame(context.SceneCamera);

			for (const auto light : lights)
			{
				Rendering::Shadows::RenderPassLight(light, m_RenderingBatch);
			}
		}

		// Render depth pre-pass
		RenderDepthPrepass(context);

		Rendering::AmbientOcclusion::Run(context);

		return;
	}

	// Render the final pass
	RenderScene(lights, context);
}

PipelineConfiguration & Pipeline3D::GetPipelineConfiguration()
{
	return m_Configuration;
}

Graphics::ITexture * Pipeline3D::GetPositionTexture()
{
	return m_DepthBuffer->GetColorBuffer();
}
