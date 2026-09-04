#include "SceneProcessor.hpp"

#include <limits>

#include "Pine/Performance/Performance.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "SceneLightsProcessor/SceneLightsProcessing.hpp"

namespace
{
    // Recompute the object's world-space bounds from the model's local bounding box.
    //
    // Deliberately built from the transform's position/rotation/scale rather than its
    // transformation matrix: those accessors resolve the parent chain from the local fields and are
    // correct whenever asked, whereas the matrix is only rebuilt in Transform::OnRender - which in
    // production mode does not run until RenderBatch, long after culling needs this.
    void UpdateWorldBounds(Pine::ModelRenderer& modelRenderer)
    {
        auto& data = modelRenderer.GetRenderingHintData();

        const auto* model = modelRenderer.GetModel();
        const auto* transform = modelRenderer.GetTransform();

        const auto position = transform->GetPosition();
        const auto rotation = transform->GetRotation();
        const auto scale = transform->GetScale();

        const auto localMin = model->GetBoundingBoxMin();
        const auto localMax = model->GetBoundingBoxMax();

        auto worldMin = Pine::Vector3f(std::numeric_limits<float>::max());
        auto worldMax = Pine::Vector3f(std::numeric_limits<float>::lowest());

        // Rotate all eight corners and take their extents. Rotating the min/max pair alone would be
        // wrong for anything not axis-aligned, which is what the old culling test got wrong.
        for (int corner = 0; corner < 8; corner++)
        {
            const auto localCorner = Pine::Vector3f(
                corner & 1 ? localMax.x : localMin.x,
                corner & 2 ? localMax.y : localMin.y,
                corner & 4 ? localMax.z : localMin.z);

            const auto worldCorner = position + rotation * (localCorner * scale);

            worldMin = glm::min(worldMin, worldCorner);
            worldMax = glm::max(worldMax, worldCorner);
        }

        data.PreviousBoundsMin = data.BoundsMin;
        data.PreviousBoundsMax = data.BoundsMax;

        data.BoundsMin = worldMin;
        data.BoundsMax = worldMax;
    }

    // Whether the object's bounds changed since last frame.
    //
    // Exact comparison on purpose. These are recomputed from the same inputs by the same code every
    // frame, so an object that did not move reproduces its bounds bit-for-bit; an epsilon would only
    // buy the ability to miss a genuinely small movement.
    bool HasBoundsChanged(const Pine::Renderer3D::ModelRendererHintData& data)
    {
        return data.BoundsMin != data.PreviousBoundsMin || data.BoundsMax != data.PreviousBoundsMax;
    }

    // Find and sort all active ModelRenderers in the scene. Will make sure to group together models using the
    // same mesh and material to allow for effective batch rendering. We also make sure to figure out which
    // materials will require discarding and blending.
    void PrepareRenderingBatch(Pine::Rendering::SceneProcessor::SceneProcessorContext& context)
    {
        PINE_PF_SCOPE();

        context.RenderingBatch = Pine::Rendering::ObjectBatchData();

        const std::size_t previousCasterCount = context.CasterCount;

        context.MovedCasters.clear();
        context.CasterCount = 0;
        context.CasterSetChanged = false;

        for (auto& modelRenderer : Pine::Components::Get<Pine::ModelRenderer>())
        {
            if (!modelRenderer.GetModel())
            {
                continue;
            }

            context.CasterCount++;

            UpdateWorldBounds(modelRenderer);

            if (HasBoundsChanged(modelRenderer.GetRenderingHintData()))
            {
                context.MovedCasters.push_back(&modelRenderer);
            }

            Pine::Rendering::SceneProcessor::Lights::ProcessModelRenderer(context, &modelRenderer);

            // Find out if a mesh within this model has a transparent material
            bool hasTransparentMaterial = false;
            for (const auto& mesh : modelRenderer.GetModel()->GetMeshes())
            {
                if (mesh->GetMaterial() && mesh->GetMaterial()->GetRenderingMode() == Pine::MaterialRenderingMode::Transparent)
                {
                    hasTransparentMaterial = true;
                }
            }

            const Pine::Rendering::RenderObject uniqueObject = { modelRenderer.GetModel(), modelRenderer.GetOverrideMaterial() };

            // Find out if we have a hint on how many instances this model has, we do this to avoid
            // having to re-allocate the vector too much.
            if (context.RenderingBatch.OpaqueObjects.count(uniqueObject) == 0)
            {
                if (context.ModelInstanceCountHint.count(uniqueObject) != 0)
                {
                    context.RenderingBatch.OpaqueObjects[uniqueObject].reserve(context.ModelInstanceCountHint[uniqueObject]);
                }
            }

            context.RenderingBatch.OpaqueObjects[uniqueObject].push_back({&modelRenderer, 0.f});

            if (hasTransparentMaterial)
            {
                context.RenderingBatch.BlendObjects[uniqueObject].push_back({&modelRenderer, 0.f});
            }
        }

        // Store instance count hint for the next frame
        for (const auto&[objectGroup, modelRenderers] : context.RenderingBatch.OpaqueObjects)
        {
            context.ModelInstanceCountHint[objectGroup] = modelRenderers.size();
        }

        // A count that did not change is not proof the *set* did not - one object destroyed and
        // another created in the same frame reads as no change. Anything that survives that is
        // per-object identity tracking, which is a real cost every frame to catch a case that
        // costs one stale frame; taking the stale frame is the better trade.
        context.CasterSetChanged = context.CasterCount != previousCasterCount;
    }
}

void Pine::Rendering::SceneProcessor::Prepare(SceneProcessorContext& context)
{
    PINE_PF_SCOPE();

    Lights::Prepare(context);

    PrepareRenderingBatch(context);
}

void Pine::Rendering::SceneProcessor::EndFrame()
{
    // Everything that reads Entity::IsDirty() has to have run by now - see the header. This used to
    // sit at the end of Prepare with a TODO saying it did not belong there, and the thing that made
    // that survivable - Prepare being the last scene-level work in the frame - stopped being true
    // once the local shadow pass moved in after it.
    for (const auto& entity : Entities::GetList())
    {
        entity->SetDirty(false);
    }
}
