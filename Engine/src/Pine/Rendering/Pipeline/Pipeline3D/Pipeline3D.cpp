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

	// Scratch storage for the draw list each pass below builds. One list serves all of them
	// because every pass builds it and submits it before the next one builds: nothing here holds
	// on to a list across passes, and reusing it is what keeps the per-frame rebuild from
	// allocating. A pass that ever needs to keep its order alive past its own draw needs its own.
	Rendering::DrawList m_DrawList;

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

		// Stated rather than inherited from whichever pass ran before this one: the depth written
		// here is handed to the scene pass, so the two have to rasterize the same faces or it
		// describes geometry that pass does not draw.
		Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);
		Graphics::GetGraphicsAPI()->SetFaceCullingMode(Graphics::FaceCullMode::Back);

		// The same corner of the shared buffers the scene pass draws into. The depth written here
		// is what that pass tests against, so the two have to rasterize to the same pixels; the
		// clear still covers the whole buffer, so nothing stale is left outside the corner.
		Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), Vector2i(renderingContext.Size));
		Graphics::GetGraphicsAPI()->ClearBuffers(Graphics::ColorBuffer | Graphics::DepthBuffer);

		Renderer3D::FrameReset();
		Renderer3D::SetCamera(renderingContext.SceneCamera);
		Renderer3D::UseRenderingContext(&renderingContext);

		renderSettings.OverrideShader = m_DepthShader;
		renderSettings.IgnoreShaderVersions = true;
		renderSettings.SkipMaterialInitialization = true;

		RenderTerrain(renderingContext);

		// Batched, not front to back, although this is the pass that would most obviously want it.
		//
		// Two reasons, both measured on levels/new-holm - see docs/rendering.md. The depth filled
		// here goes into this pass's own buffer, which only ambient occlusion reads, so ordering it
		// cannot reject anything in the pass that does the shading; and the scene is a modular kit,
		// so depth order takes its instancing apart - 147 draw calls become 1291 sorted exactly,
		// or 254 in four depth buckets.
		//
		// The order to switch to is DrawOrder::FrontToBack with a small DepthBuckets count, and the
		// change worth making first is giving the scene pass this buffer to test against.
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
	//
	// Exact back-to-front order, no depth buckets: blending is not commutative, so two surfaces
	// that swap places composite differently. That is the one case where the draw calls the
	// ordering costs have to be paid - see docs/rendering.md - and it is affordable here only
	// because a scene holds far less blended geometry than opaque.
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

		// Tested against the scene's depth so solid geometry still hides these, but writing none of
		// its own: a blended surface that wrote depth would reject the surfaces drawn after it,
		// which in this order are the ones in front of it.
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

		// Hand the pre-pass's depth to this pass instead of shading against an empty buffer. Every
		// opaque surface in front of another is then rejected before its fragments are shaded, and
		// it costs nothing in draw calls - the ordering does not change, the depth simply arrives
		// already filled. See docs/rendering.md for what that is worth on a real level.
		//
		// A copy rather than a shared attachment: the scene buffer is owned by RenderManager and
		// cleared by it after the pre-pass has run, so sharing the texture would mean that clear
		// wiping what this pass is here to read.
		if (context.SceneCamera != nullptr && m_DepthBuffer != nullptr)
		{
			auto* sceneBuffer = RenderManager::GetInternalFrameBuffer();
			const auto viewport = Vector4i(0, 0, static_cast<int>(context.Size.x), static_cast<int>(context.Size.y));

			sceneBuffer->Blit(m_DepthBuffer, Graphics::DepthBuffer, viewport, viewport);

			// Blit leaves the default framebuffer bound on both targets.
			sceneBuffer->Bind();
		}

		Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);

		// LessEqual, not Less: the surfaces this pass draws are the ones the pre-pass already wrote
		// depth for, and at an equal depth Less rejects every one of them. Not Equal either, which
		// would be the tighter test - anything the pre-pass did not draw (a discard material, which
		// it has no alpha test to render correctly) has to be able to write its own depth here.
		Graphics::GetGraphicsAPI()->SetDepthFunction(Graphics::TestFunction::LessEqual);

		Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);
		Graphics::GetGraphicsAPI()->SetFaceCullingMode(Graphics::FaceCullMode::Back);

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

		// Render fully opaque objects. Batched rather than front to back: this pass writes into its
		// own depth buffer rather than the pre-pass's, so ordering it buys nothing today - see
		// docs/rendering.md.
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

		// Skybox before the blended geometry, not after it. It is what a transparent surface with
		// nothing solid behind it blends against, and it writes no alpha of its own - drawing it
		// afterwards would either paint over what the blend produced or be rejected by the depth
		// the blend wrote, depending on which of the two writes depth.
		if (context.Skybox != nullptr)
		{
			Rendering::Skybox::Render(context.Skybox);
			context.Statistics.DrawCalls++;
		}

		RenderBlendedObjects(context);
	}

    // Allocated at the whole internal resolution, like every shared buffer, but filled only in the
    // context-sized corner the scene pass uses - the depth in it is handed to that pass, and depth
    // written under a different viewport would land in the wrong pixels. Ambient occlusion reads it
    // with the same viewportScale the resolve uses on the scene buffer.
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

	    // Depth-stencil rather than plain depth, although nothing here uses the stencil bits: this
	    // depth is blitted into the scene buffer, and a blit of the depth component requires both
	    // buffers to hold it in the same format. The scene buffer carries a stencil the editor's
	    // outlines need, so its depth is the packed 24_8 format and this one has to match it.
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

		// Only when it means something. A cull mode with culling switched off is harmless, but
		// stating one would suggest RasterState::FaceCulling still said something about how this
		// state rasterizes, and it does not.
		if (state.CullFaces)
		{
			graphicsApi->SetFaceCullingMode(state.FaceCulling);
		}
	}

	// Moves the rasterizer from one known state into another. Same result as ApplyRasterState,
	// without the depth bias call when both states carry the same pair - which is the case in
	// every pass except the shadow one, and those passes have depth bias switched off anyway.
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

	// Face culling is decided here rather than in Renderer3D::PrepareMesh, although it is a
	// material property like every other thing that call sets. The depth pre-pass and the shadow
	// pass both prepare meshes with SkipMaterialInitialization, which returns before the material
	// is looked at - and those passes rasterize the same geometry as the scene pass, so they have
	// to agree with it about which faces exist at all.
	//
	// The pass's own state is applied here rather than taken on trust. Nothing reads culling back
	// out of the graphics API, so if the batch only ever restored this state it would be restoring
	// a value the caller had promised and could quietly have stopped setting - and the symptom
	// would be a list that draws correctly right up to its first two-sided material. Applying it
	// costs one state change per batch and makes every switch below measurable against something
	// known. Unconditional, so an empty list leaves the rasterizer in the same place a full one
	// would.
	ApplyRasterState(rasterState.Default);

	const RasterState* appliedState = &rasterState.Default;

	std::size_t index = 0;

	while (index < items.size())
	{
		// One run: the longest stretch of items that share a mesh and a material, and so can go to
		// the GPU as a single instanced draw. In a batched list that is a whole model group; in a
		// depth-ordered one it is however many neighbours happened to line up.
		auto* mesh = items[index].MeshPtr;
		auto* material = items[index].MaterialPtr;

		std::size_t runEnd = index;
		while (runEnd < items.size() && items[runEnd].MeshPtr == mesh && items[runEnd].MaterialPtr == material)
		{
			runEnd++;
		}

		// A run shares its material, so the surface it draws is two-sided or it is not - resolved
		// the same way PrepareMesh resolves it, or an override material could make a run draw with
		// one material and be culled as another.
		const auto* surfaceMaterial = Renderer3D::ResolveMaterial(mesh, material);
		const bool isTwoSided = surfaceMaterial != nullptr &&
		    surfaceMaterial->GetRenderFace() == MaterialRenderFace::Both;

		// Both are members of the same rasterState, so this asks whether the state this run wants
		// is the one already on the rasterizer.
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

		// Anything writing its own stencil value cannot ride along in an instanced draw, so it is
		// drawn on its own once the rest of the run has gone out.
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

	// The pass carries on drawing through its own state once this returns - terrain, the skybox
	// and further lists of its own - so a run that switched away from it puts it back.
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
