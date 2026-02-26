#include "Pipeline3D.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"
#include "Pine/Rendering/Features/Shadows/Shadows.hpp"
#include "Pine/Rendering/Features/Skybox/Skybox.hpp"
#include "Pine/Rendering/Features/TerrainRenderer/TerrainRenderer.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"
#include "Pine/World/World.hpp"

namespace
{
	using namespace Pine;
	using namespace Pine::Pipeline3D;

	Shader* m_DepthShader = nullptr;
	Graphics::IFrameBuffer* m_DepthBuffer = nullptr;

    Rendering::SceneProcessor::SceneProcessorContext m_SceneContext;

	PipelineConfiguration m_Configuration;

	void RenderBatch(const Rendering::ObjectBatchMap& mapBatch, MaterialRenderingMode materialRenderingMode)
	{
	    if (materialRenderingMode == MaterialRenderingMode::Opaque)
	    {
	        for (const auto& terrainRenderer : Components::Get<TerrainRendererComponent>())
	        {
	            if (terrainRenderer.GetTerrain() == nullptr)
	                continue;

	            Rendering::TerrainRenderer::Render(&terrainRenderer);
	        }
	    }

		for (const auto& [modelGroup, objectRenderInstances] : mapBatch)
		{
			const auto model = modelGroup.ModelPtr;

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

				for (auto [renderer, distance] : objectRenderInstances)
				{
					const auto modelRenderer = renderer;

				    if (!modelRenderer->GetRenderingHintData().HasPassedFrustumCulling)
				    {
				        continue;
				    }

					modelRenderer->GetParent()->GetTransform()->OnRender(0.f);

					int modelMeshIndex = modelRenderer->GetModelMeshIndex();
					if (modelMeshIndex >= 0)
					{
						if (modelMeshIndex != meshIndex)
						{
							continue;
						}
					}

				    if (modelRenderer->GetOverrideStencilBuffer())
				    {
				        hasStencilBufferOverride = true;
				        continue;
				    }

					if (Renderer3D::AddInstance(
					    modelRenderer->GetParent()->GetTransform()->GetTransformationMatrix(),
					    &modelRenderer->GetRenderingHintData()))
					{
						Renderer3D::RenderMeshInstanced();
					}
				}

				Renderer3D::RenderMeshInstanced();

				if (hasStencilBufferOverride)
				{
					for (const auto [renderer, distance] : objectRenderInstances)
					{
					    int modelMeshIndex = renderer->GetModelMeshIndex();
					    if (modelMeshIndex >= 0)
					    {
					        if (modelMeshIndex != meshIndex)
					        {
					            continue;
					        }
					    }

						if (renderer->GetOverrideStencilBuffer())
						{
							renderer->GetParent()->GetTransform()->OnRender(0.f);

							Renderer3D::RenderMesh(
							    renderer->GetParent()->GetTransform()->GetTransformationMatrix(),
							    &renderer->GetRenderingHintData(),
							    renderer->GetStencilBufferValue());
						}
					}
				}
			}
		}
	}

	void RenderDepthPrepass(RenderingContext& renderingContext)
	{
		PINE_PF_SCOPE();

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

		RenderBatch(m_SceneContext.RenderingBatch.OpaqueObjects, MaterialRenderingMode::Opaque);

		renderSettings.OverrideShader = nullptr;
		renderSettings.IgnoreShaderVersions = false;
		renderSettings.SkipMaterialInitialization = false;
	}

	void RenderScene(const std::vector<Light*>& lights, RenderingContext& context)
	{
		PINE_PF_SCOPE();

		Renderer3D::FrameReset();

		if (context.SceneCamera)
        {
            Renderer3D::SetCamera(context.SceneCamera);

	        Rendering::RenderCulling::RunFrustumCulling(context.SceneCamera);
        }

	    const auto& levelSettings = World::GetActiveLevel()->GetLevelSettings();

        Renderer3D::UseRenderingContext(&context);

		Renderer3D::PrepareScene(
		    levelSettings.AmbientColor,
		    levelSettings.FogColor,
		    levelSettings.FogDistance,
		    levelSettings.FogIntensity);

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
		RenderBatch(m_SceneContext.RenderingBatch.OpaqueObjects, MaterialRenderingMode::Opaque);

		// Render objects which require discarding
		RenderBatch(m_SceneContext.RenderingBatch.OpaqueObjects, MaterialRenderingMode::Discard);

		// TODO: Render semi-transparent objects, we'll have to sort all objects by distance as well.

		// Skybox
		if (context.Skybox != nullptr)
		{
			Rendering::Skybox::Render(context.Skybox);
			context.Statistics.DrawCalls++;
		}
	}

    void CreateDepthBuffer()
	{
	    m_DepthBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
	    m_DepthBuffer->Bind();
	    m_DepthBuffer->Prepare();

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
	}
}

void Pipeline3D::Setup()
{
	Rendering::Skybox::Setup();
	Rendering::Shadows::Setup();
	Rendering::AmbientOcclusion::Setup();

	CreateDepthBuffer();

	Rendering::AmbientOcclusion::UseDepthBuffer(m_DepthBuffer);

	m_DepthShader = Assets::Get<Shader>("engine/shaders/3d/depth");
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
	PINE_PF_SCOPE();

    Rendering::SceneProcessor::Prepare(m_SceneContext);
}

void Pipeline3D::Run(RenderingContext& context, PipelineStage stage)
{
	if (stage == PipelineStage::Prepass)
	{
		PINE_PF_SCOPE_MANUAL("Pine::Pipeline3D::Run(PipelineStage::Prepass)");

		// Render shadow pass
		if (m_Configuration.RenderShadows)
		{
			Rendering::Shadows::NewFrame(context.SceneCamera);

			for (const auto light : m_SceneContext.Lights)
			{
				Rendering::Shadows::RenderPassLight(light, m_SceneContext.RenderingBatch);
			}
		}

		// Render depth pre-pass
		RenderDepthPrepass(context);

		Rendering::AmbientOcclusion::Run(context);

		return;
	}

	PINE_PF_SCOPE_MANUAL("Pine::Pipeline3D::Run(PipelineStage::Default)");

	// Render the final pass
	RenderScene(m_SceneContext.Lights, context);
}

PipelineConfiguration & Pipeline3D::GetPipelineConfiguration()
{
	return m_Configuration;
}

Graphics::ITexture * Pipeline3D::GetPositionTexture()
{
	return m_DepthBuffer->GetColorBuffer();
}
