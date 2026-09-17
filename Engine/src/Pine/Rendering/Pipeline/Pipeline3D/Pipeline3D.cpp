#include "Pipeline3D.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"
#include "Pine/Rendering/Features/Shadows/Shadows.hpp"
#include "Pine/Rendering/Features/Skybox/Skybox.hpp"
#include "Pine/Rendering/Features/TerrainRenderer/TerrainRenderer.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"
#include "Pine/World/World.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
	using namespace Pine;
	using namespace Pine::Pipeline3D;

	Shader* m_DepthShader = nullptr;
	Graphics::IFrameBuffer* m_DepthBuffer = nullptr;

    Rendering::SceneProcessor::SceneProcessorContext m_SceneContext;

	PipelineConfiguration m_Configuration;

	// Terrain is not part of the object batch - it renders through its own path, with its own mesh
	// per chunk and its own detail level per viewer, so it cannot be expressed as a model group.
	//
	// The view it is handed is this context's: its frustum culls the chunks, and its camera picks
	// their detail levels. Without a camera there is no frustum to cull against this frame, and the
	// one left over from the last camera this context had would hide arbitrary chunks.
	void RenderTerrain(RenderingContext& renderingContext)
	{
	    if (renderingContext.SceneCamera == nullptr)
	    {
	        return;
	    }

	    Rendering::TerrainRenderer::BeginPass(renderingContext);

	    Rendering::TerrainRenderer::Render({
	        renderingContext.ViewFrustum,
	        renderingContext.SceneCamera->GetParent()->GetTransform()->GetPosition(),
	        &renderingContext.Statistics
	    });
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

		RenderTerrain(renderingContext);
		RenderBatch(m_SceneContext.RenderingBatch.OpaqueObjects, MaterialRenderingMode::Opaque, renderingContext.Visibility);

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

		Graphics::GetGraphicsAPI()->SetBlendingEnabled(false);
		Graphics::GetGraphicsAPI()->SetBlendingFunction(Graphics::BlendingFunction::SourceAlpha, Graphics::BlendingFunction::OneMinusSourceAlpha);

		// No separate shadow upload any more: a light's shadow views reach the shader through its
		// own entry in the light buffer, which AddLight already writes. The directional light used
		// to need a second call here to hand over a texture nothing else could see.
		for (const auto light : lights)
		{
			Renderer3D::AddLight(light);
		}

		Renderer3D::UploadLights();

		RenderTerrain(context);

		// Render fully opaque objects.
		RenderBatch(m_SceneContext.RenderingBatch.OpaqueObjects, MaterialRenderingMode::Opaque, context.Visibility);

		// Render objects which require discarding
		RenderBatch(m_SceneContext.RenderingBatch.OpaqueObjects, MaterialRenderingMode::Discard, context.Visibility);

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
            0,
            Graphics::TextureFormat::RGBA16F,
            Graphics::TextureDataFormat::Float,
            nullptr);

	    m_DepthBuffer->AttachTexture(normalBuffer, Graphics::BufferAttachment::Color);

	    const auto depthBuffer = Graphics::GetGraphicsAPI()->CreateTexture();

	    depthBuffer->Bind();
	    depthBuffer->UploadTextureData(
            Renderer3D::Specifications::General::INTERNAL_WIDTH,
            Renderer3D::Specifications::General::INTERNAL_HEIGHT,
            0,
            Graphics::TextureFormat::Depth, Graphics::TextureDataFormat::Float,
            nullptr);

	    m_DepthBuffer->AttachTexture(depthBuffer, Graphics::BufferAttachment::Depth);
	    m_DepthBuffer->Finish();
	}
}

void Pipeline3D::RenderBatch(const Rendering::ObjectBatchMap& mapBatch,
                             const MaterialRenderingMode materialRenderingMode,
                             const Rendering::RenderCulling::VisibilitySet& visibility)
{
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

			    if (!visibility.IsVisible(modelRenderer->GetInternalId()))
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
				    &modelRenderer->GetRenderingHintData().Lights))
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
						    &renderer->GetRenderingHintData().Lights,
						    renderer->GetStencilBufferValue());
					}
				}
			}
		}
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

	// Before any context draws, and once for all of them - a terrain's meshes depend on its height
	// field, and its chunk light slots on where the lights are, neither of which is about the viewer.
	//
	// After SceneProcessor::Prepare, which is what gathered the lights the chunks are assigned from.
	Rendering::TerrainRenderer::Prepare(m_SceneContext);

	// Local light shadows are viewer-independent, so they are built and rendered once here rather
	// than inside each rendering context's prepass. With an editor viewport and a game camera both
	// live, doing it per context would render every spot light's shadow map twice per frame for an
	// identical result.
	if (m_Configuration.RenderShadows)
	{
		Rendering::Shadows::PrepareLocalViews(m_SceneContext);
		Rendering::Shadows::RenderLocalViews(m_SceneContext.RenderingBatch);
	}
	else
	{
		Rendering::Shadows::ClearLocalViews(m_SceneContext.Lights);
	}

	// After the scene-level shadow work, not inside SceneProcessor::Prepare where it used to live
	// behind a TODO. Prepare is no longer the last thing to look at the scene each frame, so the
	// flags have to outlive it. This ordering is load-bearing - see SceneProcessor::EndFrame.
	Rendering::SceneProcessor::EndFrame();
}

void Pipeline3D::Run(RenderingContext& context, const PipelineStage stage)
{
	if (stage == PipelineStage::Prepass)
	{
		PINE_PF_SCOPE_MANUAL("Pine::Pipeline3D::Run(PipelineStage::Prepass)");

		// Visibility depends on the camera, and *both* stages consume it - the depth pre-pass below
		// skips culled objects, and so does RenderScene in the Default stage. Culling here, into this
		// context's own set, is what keeps the two stages agreeing and keeps two viewports from
		// overwriting each other's results.
		if (context.SceneCamera != nullptr)
		{
			// Kept on the context rather than local to this block: terrain culls its chunks
			// against the same frustum in both stages, and rebuilding it there would be a second
			// expression that has to agree with this one.
			context.ViewFrustum = Frustum::FromViewProjection(
				context.SceneCamera->GetProjectionMatrix() * context.SceneCamera->GetViewMatrix());

			const auto cullingResult = Rendering::RenderCulling::Cull(context.ViewFrustum, context.Visibility);

			context.Statistics.VisibleObjectCount = cullingResult.VisibleObjectCount;
			context.Statistics.CulledObjectCount = cullingResult.CulledObjectCount;
		}

		// Render shadow pass
		if (m_Configuration.RenderShadows)
		{
			Rendering::Shadows::NewFrame(context.SceneCamera);

			for (const auto light : m_SceneContext.Lights)
			{
				Rendering::Shadows::RenderPassLight(light, m_SceneContext);
			}
		}

		// Render depth pre-pass
		RenderDepthPrepass(context);

		// When disabled the AO output buffer stays cleared to white (see
		// AmbientOcclusion::Setup), so the post-process multiply is a no-op.
		if (m_Configuration.RenderAmbientOcclusion)
		{
			Rendering::AmbientOcclusion::Run(context);
		}

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
