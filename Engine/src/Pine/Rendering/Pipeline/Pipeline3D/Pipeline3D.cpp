#include "Pipeline3D.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Features/Shadows/Shadows.hpp"
#include "Pine/Rendering/Features/Skybox/Skybox.hpp"

namespace
{
	using namespace Pine;
	using namespace Pine::Pipeline3D;

	PipelineConfiguration m_Configuration;

	std::unordered_map<RenderObject, std::uint32_t, RenderObjectHash> m_ModelInstanceCountHint;

	// Find and sort all active ModelRenderers in the scene. Will make sure to group together models using the
	// same mesh and material, to allow for effective batch rendering. We also make sure to figure out which
	// materials will require discarding and blending.
	ObjectBatchData PrepareRenderingBatch()
	{
		ObjectBatchData renderBatch;

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
			if (renderBatch.OpaqueObjects.count(uniqueObject) == 0)
			{
				if (m_ModelInstanceCountHint.count(uniqueObject) != 0)
				{
					renderBatch.OpaqueObjects[uniqueObject].reserve(m_ModelInstanceCountHint[uniqueObject]);
				}
			}

			renderBatch.OpaqueObjects[uniqueObject].push_back({&modelRenderer, 0.f});

			if (hasTransparentMaterial)
			{
				renderBatch.BlendObjects[uniqueObject].push_back({&modelRenderer, 0.f});
			}
		}

		// Store instance count hint for the next frame
		for (const auto&[objectGroup, modelRenderers] : renderBatch.OpaqueObjects)
		{
			m_ModelInstanceCountHint[objectGroup] = modelRenderers.size();
		}

		return renderBatch;
	}

	void RenderBatch(const ObjectBatchMap& mapBatch, MaterialRenderingMode materialRenderingMode)
	{
		for (const auto& [modelGroup, objectRenderInstances] : mapBatch)
		{
			const auto model = modelGroup.RenderModel;

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

	void RenderScene(const ObjectBatchData& batchData, const std::vector<Light*>& lights, RenderingContext& context)
	{
		Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);
		Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);

		Graphics::GetGraphicsAPI()->SetBlendingEnabled(true);
		Graphics::GetGraphicsAPI()->SetBlendingFunction(Graphics::BlendingFunction::SourceAlpha, Graphics::BlendingFunction::OneMinusSourceAlpha);

		for (const auto light : lights)
		{
			Renderer3D::AddLight(light);
		}

		Renderer3D::UploadLights();

		// Render fully opaque objects.
		RenderBatch(batchData.OpaqueObjects, MaterialRenderingMode::Opaque);

		// Render objects which require discarding
		RenderBatch(batchData.OpaqueObjects, MaterialRenderingMode::Discard);

		// TODO: Render semi-transparent objects, we'll have to sort all objects by distance as well.

		// Skybox
		if (context.Skybox != nullptr)
		{
			Renderer::Skybox::Render(context.Skybox);
			context.DrawCalls++;
		}
	}
}

void Pipeline3D::Setup()
{
	Renderer::Skybox::Setup();
}

void Pipeline3D::Shutdown()
{
	Renderer::Skybox::Shutdown();
}

void Pipeline3D::Run(RenderingContext& context)
{
	Renderer3D::FrameReset();

	if (context.SceneCamera)
		Renderer3D::SetCamera(context.SceneCamera);

	Renderer3D::UseRenderingContext(&context);

	// Prepare Data
	const auto renderingBatch = PrepareRenderingBatch();
	const auto lights = GetLights();

	// Render shadow pass
	if (m_Configuration.RenderShadows)
	{
		Rendering::Shadows::NewFrame();

		for (const auto light : lights)
		{
			//Rendering::Shadows::RenderPassLight();
		}
	}

	// Render depth pre-pass

	// Render final pass
	RenderScene(renderingBatch, lights, context);
}

PipelineConfiguration & Pipeline3D::GetPipelineConfiguration()
{
	return m_Configuration;
}
