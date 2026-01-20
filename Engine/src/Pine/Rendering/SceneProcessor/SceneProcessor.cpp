#include "SceneProcessor.hpp"

#include "Pine/Performance/Performance.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "SceneLightsProcessor/SceneLightsProcessing.hpp"

namespace
{
    // Find and sort all active ModelRenderers in the scene. Will make sure to group together models using the
    // same mesh and material to allow for effective batch rendering. We also make sure to figure out which
    // materials will require discarding and blending.
    void PrepareRenderingBatch(Pine::Rendering::SceneProcessor::SceneProcessorContext& context)
    {
        PINE_PF_SCOPE();

        context.RenderingBatch = Pine::Rendering::ObjectBatchData();

        for (auto& modelRenderer : Pine::Components::Get<Pine::ModelRenderer>())
        {
            if (!modelRenderer.GetModel())
            {
                continue;
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
    }
}

void Pine::Rendering::SceneProcessor::Prepare(SceneProcessorContext& context)
{
    PINE_PF_SCOPE();

    Lights::Prepare(context);

    PrepareRenderingBatch(context);

    // TODO: This should really not be done here, since it could be used by other engine components.
    // Right now it will sort of work since the rendering is done last anyway, but it's not ideal.
    for (const auto& entity : Entities::GetList())
    {
        entity->SetDirty(false);
    }
}
