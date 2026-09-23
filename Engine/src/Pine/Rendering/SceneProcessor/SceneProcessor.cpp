#include "SceneProcessor.hpp"

#include <algorithm>

#include "Pine/Performance/Performance.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "SceneLightsProcessor/SceneLightsProcessing.hpp"

namespace
{
    // Recompute the object's world-space bounds from the model's local bounding box.
    //
    // Built from the transform's position/rotation/scale rather than its transformation matrix,
    // which in production mode is not rebuilt until RenderBatch, after culling needs this.
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

        // The transform's linear part: the rotation with the scale folded into its columns.
        // Negative scale needs no special case.
        auto linear = glm::mat3_cast(rotation);

        linear[0] *= scale.x;
        linear[1] *= scale.y;
        linear[2] *= scale.z;

        // Equivalent to the extents of all eight transformed corners. See Math::TransformBounds.
        Pine::Vector3f worldMin;
        Pine::Vector3f worldMax;

        Pine::Math::TransformBounds(linear, position, localMin, localMax, worldMin, worldMax);

        data.PreviousBoundsMin = data.BoundsMin;
        data.PreviousBoundsMax = data.BoundsMax;

        data.BoundsMin = worldMin;
        data.BoundsMax = worldMax;
    }

    // Whether the object's bounds changed since last frame. Compared exactly: an object that did
    // not move reproduces its bounds bit-for-bit.
    bool HasBoundsChanged(const Pine::Renderer3D::ModelRendererHintData& data)
    {
        return data.BoundsMin != data.PreviousBoundsMin || data.BoundsMax != data.PreviousBoundsMax;
    }

    // The model this renderer draws this frame, measured from the reference position to the
    // middle of its world bounds. See Model::SelectLod.
    Pine::Model* SelectLodModel(Pine::ModelRenderer& modelRenderer, const std::optional<Pine::Vector3f>& referencePosition)
    {
        const auto model = modelRenderer.GetModel();

        if (model->GetLodLevels().empty() && model->GetLodCullDistance() <= 0.f)
        {
            return model;
        }

        // A renderer drawing one mesh of its model picks that mesh by index, and the index means
        // nothing in the other levels' models.
        if (!referencePosition.has_value() || modelRenderer.GetModelMeshIndex() >= 0)
        {
            return model;
        }

        const auto& data = modelRenderer.GetRenderingHintData();
        const auto centre = (data.BoundsMin + data.BoundsMax) * 0.5f;

        // Divided by the largest axis, so an object scaled up keeps its detail proportionally
        // further out, and the distances set on the model hold for every copy of it.
        const auto scale = glm::abs(modelRenderer.GetTransform()->GetScale());
        const float largestScale = std::max({ scale.x, scale.y, scale.z });

        if (largestScale <= 0.f)
        {
            return model;
        }

        const float scaledDistance = glm::length(centre - referencePosition.value()) / largestScale;

        return model->SelectLod(scaledDistance);
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

            auto& hintData = modelRenderer.GetRenderingHintData();

            const auto previousLodModel = hintData.LodModel;
            hintData.LodModel = SelectLodModel(modelRenderer, context.LodReferencePosition);

            if (HasBoundsChanged(hintData) || hintData.LodModel != previousLodModel)
            {
                context.MovedCasters.push_back(&modelRenderer);
            }

            // Kept up to date while the object is hidden by distance as well. The slot cache is
            // invalidated through per-frame dirty flags, so an object that moved while hidden would
            // otherwise come back lit by wherever it used to be.
            Pine::Rendering::SceneProcessor::Lights::ProcessModelRenderer(context, &modelRenderer);

            const auto model = hintData.LodModel;

            // Past its model's cull distance.
            if (model == nullptr)
            {
                continue;
            }

            // Resolved the way the draw list resolves it: an override material replaces every
            // mesh's own.
            auto* overrideMaterial = modelRenderer.GetOverrideMaterial();

            bool hasTransparentMaterial = false;
            for (const auto& mesh : model->GetMeshes())
            {
                const auto* material = overrideMaterial != nullptr ? overrideMaterial : mesh->GetMaterial();

                if (material && material->GetRenderingMode() == Pine::MaterialRenderingMode::Transparent)
                {
                    hasTransparentMaterial = true;
                }
            }

            const Pine::Rendering::RenderObject uniqueObject = { model, overrideMaterial };

            // Find out if we have a hint on how many instances this model has, we do this to avoid
            // having to re-allocate the vector too much.
            if (context.RenderingBatch.OpaqueObjects.count(uniqueObject) == 0)
            {
                if (context.ModelInstanceCountHint.count(uniqueObject) != 0)
                {
                    context.RenderingBatch.OpaqueObjects[uniqueObject].reserve(context.ModelInstanceCountHint[uniqueObject]);
                }
            }

            context.RenderingBatch.OpaqueObjects[uniqueObject].push_back({&modelRenderer});

            if (hasTransparentMaterial)
            {
                context.RenderingBatch.BlendObjects[uniqueObject].push_back({&modelRenderer});
            }
        }

        // Store instance count hint for the next frame
        for (const auto&[objectGroup, modelRenderers] : context.RenderingBatch.OpaqueObjects)
        {
            context.ModelInstanceCountHint[objectGroup] = modelRenderers.size();
        }

        // Misses one object destroyed and another created in the same frame. That costs one stale
        // frame, which is cheaper than tracking identity every frame.
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
    for (const auto& entity : Entities::GetList())
    {
        entity->SetDirty(false);
    }
}
