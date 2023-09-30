#include "Pipeline3D.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/Graphics/Graphics.hpp"

namespace
{
	using namespace Pine;

	__inline bool IsComponentValid(const IComponent* component)
	{
		return component && component->GetActive() && component->GetParent()->GetActive();
	}

	std::unordered_map<Model*, std::vector<ModelRenderer*>> GetRenderingBatch()
	{
		std::unordered_map<Model*, std::vector<ModelRenderer*>> renderBatch;

		for (auto& modelRenderer : Components::Get<ModelRenderer>())
		{
			if (!IsComponentValid(&modelRenderer) && !modelRenderer.GetModel())
			{
				continue;
			}

			renderBatch[modelRenderer.GetModel()].push_back(&modelRenderer);
		}

		return renderBatch;
	}

	std::vector<Light*> GetLights()
	{
		std::vector<Light*> lights;

		for (auto& light : Components::Get<Light>())
		{
			if (!IsComponentValid(&light))
			{
				continue;
			}

			lights.push_back(&light);
		}

		return lights;
	}
}

void Pipeline3D::Setup()
{
}

void Pipeline3D::Shutdown()
{
}

void Pipeline3D::Run(const RenderingContext& context)
{
	if (context.SceneCamera)
		Renderer3D::SetCamera(context.SceneCamera);

	Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);
	Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);

	// Prepare Data
	const auto renderingBatch = GetRenderingBatch();
	const auto lights = GetLights();

	// Upload Data
	for (auto light : lights)
	{
		Renderer3D::AddLight(light);
	}

	Renderer3D::UploadLights();

	// Rendering
	for (const auto& [model, renderers] : renderingBatch)
	{
		for (auto mesh : model->GetMeshes())
		{
			Renderer3D::PrepareMesh(mesh);

			// If we have multiple entities to render, use instanced rendering.
			if (renderers.size() > 1)
			{
				for (auto renderer : renderers)
				{
					renderer->GetParent()->GetTransform()->OnRender(0.f);

					if (Renderer3D::AddInstance(renderer->GetParent()->GetTransform()->GetTransformationMatrix()))
					{
						Renderer3D::RenderMeshInstanced();
					}
				}

				Renderer3D::RenderMeshInstanced();
			}
			else
			{
				auto renderer = renderers[0];

				renderer->GetParent()->GetTransform()->OnRender(0.f);

				Renderer3D::RenderMesh(renderer->GetParent()->GetTransform()->GetTransformationMatrix());
			}
		}
	}

	// Sky box
}
