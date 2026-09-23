#include "Pipeline3D.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"
#include "Pine/Rendering/Features/Shadows/Shadows.hpp"
#include "Pine/Rendering/Features/Skybox/Skybox.hpp"
#include "Pine/Rendering/Features/TerrainDetail/TerrainDetail.hpp"
#include "Pine/Rendering/Features/TerrainRenderer/TerrainRenderer.hpp"
#include "Pine/Rendering/InternalResolution/InternalResolution.hpp"
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

	// Scratch draw list, rebuilt and submitted by each pass in turn.
	Rendering::DrawList m_DrawList;

	PipelineConfiguration m_Configuration;

	// Where LOD distances are measured from: the camera of the first context that draws the scene
	// this frame. One position for the whole frame, so a second viewport open at the same time
	// shows the levels chosen for the first.
	std::optional<Vector3f> FindLodReferencePosition()
	{
		for (const auto context : RenderManager::GetRenderingContexts())
		{
			if (context == nullptr || !context->Active || !context->UseRenderPipeline || context->SceneCamera == nullptr)
			{
				continue;
			}

			return context->SceneCamera->GetParent()->GetTransform()->GetPosition();
		}

		return std::nullopt;
	}

	// Terrain is not part of the object batch; it culls and picks detail levels against this
	// context's frustum and camera. Without a camera the frustum is left over from an earlier one.
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

		// Must match the scene pass, which this depth is handed to.
		Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);
		Graphics::GetGraphicsAPI()->SetFaceCullingMode(Graphics::FaceCullMode::Back);

		// The same corner of the shared buffers the scene pass draws into, so the depth lands in the
		// pixels it tests.
		Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), Vector2i(renderingContext.Size));
		Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer | Graphics::DepthBuffer);

		Renderer3D::FrameReset();
		Renderer3D::SetCamera(renderingContext.SceneCamera);
		Renderer3D::UseRenderingContext(&renderingContext);

		renderSettings.OverrideShader = m_DepthShader;
		renderSettings.IgnoreShaderVersions = true;
		renderSettings.SkipMaterialInitialization = true;

		RenderTerrain(renderingContext);

		// Batched: the scene pass already gets this depth, so ordering would only speed up the
		// pre-pass's own writes, at a large cost in draw calls. See docs/rendering.md.
		m_DrawList.Build(m_SceneContext.RenderingBatch.OpaqueObjects,
		    MaterialRenderingMode::Opaque,
		    renderingContext.Visibility,
		    { Rendering::DrawOrder::Batched });

		RenderBatch(m_DrawList);

		renderSettings.OverrideShader = nullptr;
		renderSettings.IgnoreShaderVersions = false;
		renderSettings.SkipMaterialInitialization = false;
	}

	// Everything with a Transparent material, blended over the scene that is already in the buffer.
	// Exact back-to-front order, since blending is not commutative.
	void RenderBlendedObjects(RenderingContext& context)
	{
		PINE_PF_SCOPE();

		if (context.SceneCamera == nullptr)
		{
			return;
		}

		m_DrawList.Build(m_SceneContext.RenderingBatch.BlendObjects,
		    MaterialRenderingMode::Transparent,
		    context.Visibility,
		    { Rendering::DrawOrder::BackToFront,
		      context.SceneCamera->GetParent()->GetTransform()->GetPosition() });

		if (m_DrawList.GetItems().empty())
		{
			return;
		}

		auto* graphicsApi = Graphics::GetGraphicsAPI();

		// Depth-tested but not written: in this order, a surface drawn later is in front.
		graphicsApi->SetDepthFunction(Graphics::TestFunction::LessEqual);
		graphicsApi->SetDepthWriteEnabled(false);
		graphicsApi->SetBlendingEnabled(true);

		RenderBatch(m_DrawList);

		graphicsApi->SetBlendingEnabled(false);
		graphicsApi->SetDepthWriteEnabled(true);
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

		// Start from the pre-pass's depth, so hidden opaque fragments are rejected before shading. A
		// copy rather than a shared attachment, because RenderManager clears the scene buffer after
		// the pre-pass has run. See docs/rendering.md.
		if (context.SceneCamera != nullptr && m_DepthBuffer != nullptr)
		{
			auto* sceneBuffer = RenderManager::GetInternalFrameBuffer();
			const auto viewport = Vector4i(0, 0, static_cast<int>(context.Size.x), static_cast<int>(context.Size.y));

			sceneBuffer->Blit(m_DepthBuffer, Graphics::DepthBuffer, viewport, viewport);

			// Blit leaves the default framebuffer bound on both targets.
			sceneBuffer->Bind();
		}

		Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);

		// LessEqual: Less would reject the surfaces the pre-pass already wrote, and Equal would reject
		// Discard materials, which the pre-pass does not draw.
		Graphics::GetGraphicsAPI()->SetDepthFunction(Graphics::TestFunction::LessEqual);

		Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);
		Graphics::GetGraphicsAPI()->SetFaceCullingMode(Graphics::FaceCullMode::Back);

		Graphics::GetGraphicsAPI()->SetBlendingEnabled(false);
		Graphics::GetGraphicsAPI()->SetBlendingFunction(Graphics::BlendingFunction::SourceAlpha, Graphics::BlendingFunction::OneMinusSourceAlpha);

		for (const auto light : lights)
		{
			Renderer3D::AddLight(light);
		}

		Renderer3D::UploadLights();

		RenderTerrain(context);

		// Render fully opaque objects. Batched, since the pre-pass depth already rejects hidden
		// fragments.
		m_DrawList.Build(m_SceneContext.RenderingBatch.OpaqueObjects,
		    MaterialRenderingMode::Opaque,
		    context.Visibility,
		    { Rendering::DrawOrder::Batched });

		RenderBatch(m_DrawList);

		// Render objects which require discarding
		m_DrawList.Build(m_SceneContext.RenderingBatch.OpaqueObjects,
		    MaterialRenderingMode::Discard,
		    context.Visibility,
		    { Rendering::DrawOrder::Batched });

		RenderBatch(m_DrawList);

		// Alpha-tested like the Discard batch above, and for the same reason absent from the pre-pass.
		Rendering::TerrainDetail::Render(context);

		// Skybox before the blended geometry, which blends against it.
		if (context.Skybox != nullptr)
		{
			Rendering::Skybox::Render(context.Skybox);
			context.Statistics.DrawCalls++;
		}

		RenderBlendedObjects(context);
	}

    // Allocated at the whole internal resolution, like every shared buffer, but filled only in the
    // context-sized corner the scene pass uses.
    void CreateDepthBuffer()
	{
	    const auto resolution = Rendering::InternalResolution::Get();

	    m_DepthBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
	    m_DepthBuffer->Bind();
	    m_DepthBuffer->Prepare();

	    const auto normalBuffer = Graphics::GetGraphicsAPI()->CreateTexture();

	    normalBuffer->Bind();
	    normalBuffer->UploadTextureData(
            resolution.x,
            resolution.y,
            0,
            Graphics::TextureFormat::RGBA16F,
            Graphics::TextureDataFormat::Float,
            nullptr);

	    m_DepthBuffer->AttachTexture(normalBuffer, Graphics::BufferAttachment::Color);

	    // Depth-stencil, although nothing here uses the stencil: a depth blit needs both buffers in
	    // the same format, and the scene buffer's depth is packed 24_8 for the editor's outlines.
	    const auto depthBuffer = Graphics::GetGraphicsAPI()->CreateTexture();

	    depthBuffer->Bind();
	    depthBuffer->UploadTextureData(
            resolution.x,
            resolution.y,
            0,
            Graphics::TextureFormat::DepthStencil, Graphics::TextureDataFormat::UnsignedInt24_8,
            nullptr);

	    m_DepthBuffer->AttachTexture(depthBuffer, Graphics::BufferAttachment::DepthStencil);
	    m_DepthBuffer->Finish();
	}

	void ApplyFaceCulling(const Pipeline3D::RasterState& state)
	{
		auto* graphicsApi = Graphics::GetGraphicsAPI();

		graphicsApi->SetFaceCullingEnabled(state.CullFaces);

		if (state.CullFaces)
		{
			graphicsApi->SetFaceCullingMode(state.FaceCulling);
		}
	}

	// Same result as ApplyRasterState, skipping the depth bias call when it would not change.
	void SwitchRasterState(const Pipeline3D::RasterState& from, const Pipeline3D::RasterState& to)
	{
		ApplyFaceCulling(to);

		if (to.SlopeBias != from.SlopeBias || to.DepthBias != from.DepthBias)
		{
			Graphics::GetGraphicsAPI()->SetDepthBias(to.SlopeBias, to.DepthBias);
		}
	}
}

void Pipeline3D::ApplyRasterState(const RasterState& state)
{
	ApplyFaceCulling(state);

	Graphics::GetGraphicsAPI()->SetDepthBias(state.SlopeBias, state.DepthBias);
}

void Pipeline3D::RenderBatch(const Rendering::DrawList& drawList, const BatchRasterState& rasterState)
{
	const auto& items = drawList.GetItems();

	// Face culling is decided here rather than in Renderer3D::PrepareMesh, because the pre-pass and
	// shadow pass skip material initialization there but must still cull like the scene pass.
	//
	// The default state is applied up front rather than assumed, since nothing reads culling back
	// from the graphics API.
	ApplyRasterState(rasterState.Default);

	const RasterState* appliedState = &rasterState.Default;

	std::size_t index = 0;

	while (index < items.size())
	{
		// One run: the longest stretch of items sharing a mesh and a material, drawn as one
		// instanced draw.
		auto* mesh = items[index].MeshPtr;
		auto* material = items[index].MaterialPtr;

		std::size_t runEnd = index;
		while (runEnd < items.size() && items[runEnd].MeshPtr == mesh && items[runEnd].MaterialPtr == material)
		{
			runEnd++;
		}

		// Resolved the same way PrepareMesh resolves it, so an override material is culled as drawn.
		const auto* surfaceMaterial = Renderer3D::ResolveMaterial(mesh, material);
		const bool isTwoSided = surfaceMaterial != nullptr &&
		    surfaceMaterial->GetRenderFace() == MaterialRenderFace::Both;

		const RasterState& runState = isTwoSided ? rasterState.TwoSided : rasterState.Default;

		if (&runState != appliedState)
		{
			SwitchRasterState(*appliedState, runState);

			appliedState = &runState;
		}

		Renderer3D::PrepareMesh(mesh, material);

		bool hasStencilBufferOverride = false;

		for (std::size_t i = index; i < runEnd; i++)
		{
			const auto modelRenderer = items[i].Renderer;

			modelRenderer->GetParent()->GetTransform()->OnRender(0.f);

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

		// Anything writing its own stencil value cannot be instanced, so it is drawn separately.
		if (hasStencilBufferOverride)
		{
			for (std::size_t i = index; i < runEnd; i++)
			{
				const auto modelRenderer = items[i].Renderer;

				if (!modelRenderer->GetOverrideStencilBuffer())
				{
					continue;
				}

				modelRenderer->GetParent()->GetTransform()->OnRender(0.f);

				Renderer3D::RenderMesh(
				    modelRenderer->GetParent()->GetTransform()->GetTransformationMatrix(),
				    &modelRenderer->GetRenderingHintData().Lights,
				    modelRenderer->GetStencilBufferValue());
			}
		}

		index = runEnd;
	}

	// The pass keeps drawing in its default state after this returns.
	if (appliedState != &rasterState.Default)
	{
		SwitchRasterState(*appliedState, rasterState.Default);
	}
}

void Pipeline3D::Setup()
{
	Rendering::Skybox::Setup();
	Rendering::Shadows::Setup();
	Rendering::AmbientOcclusion::Setup();

	CreateDepthBuffer();

	Rendering::AmbientOcclusion::UseDepthBuffer(m_DepthBuffer);

	Rendering::InternalResolution::AddResizeCallback([]
	{
		Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_DepthBuffer);

		CreateDepthBuffer();

		// Ambient occlusion only borrows the buffer, so hand it the new one.
		Rendering::AmbientOcclusion::UseDepthBuffer(m_DepthBuffer);
	});

	m_DepthShader = Assets::Get<Shader>("engine/shaders/3d/depth");
}

void Pipeline3D::Shutdown()
{
	Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_DepthBuffer);

	Rendering::AmbientOcclusion::Shutdown();
	Rendering::Skybox::Shutdown();
	Rendering::Shadows::Shutdown();
	Rendering::TerrainDetail::Shutdown();
}

void Pipeline3D::Prepare()
{
	PINE_PF_SCOPE();

	m_SceneContext.LodReferencePosition = FindLodReferencePosition();

    Rendering::SceneProcessor::Prepare(m_SceneContext);

	// Once for all contexts, after SceneProcessor::Prepare has gathered the lights.
	Rendering::TerrainRenderer::Prepare(m_SceneContext);

	// After the terrain, whose chunk light slots the detail is lit through.
	Rendering::TerrainDetail::Prepare();

	// Local light shadows are viewer-independent, so they render once here rather than per context.
	if (m_Configuration.RenderShadows)
	{
		Rendering::Shadows::PrepareLocalViews(m_SceneContext);
		Rendering::Shadows::RenderLocalViews(m_SceneContext.RenderingBatch);
	}
	else
	{
		Rendering::Shadows::ClearLocalViews(m_SceneContext.Lights);
	}

	// Must stay after the shadow work. See SceneProcessor::EndFrame.
	Rendering::SceneProcessor::EndFrame();
}

void Pipeline3D::Run(RenderingContext& context, const PipelineStage stage)
{
	if (stage == PipelineStage::Prepass)
	{
		PINE_PF_SCOPE_MANUAL("Pine::Pipeline3D::Run(PipelineStage::Prepass)");

		// Culled once into this context's own set, which both stages draw from.
		if (context.SceneCamera != nullptr)
		{
			// Kept on the context, because terrain culls against it in both stages.
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
