#include "Pipeline3D.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Features/Skybox/Skybox.hpp"

namespace
{
    using namespace Pine;

    typedef std::unordered_map<Model *, std::vector<ModelRenderer *>> ObjectMapBatch;

    struct RenderBatchData
    {
        ObjectMapBatch Objects;

        // Objects which will require discarding
        ObjectMapBatch DiscardObjects;

        // Objects which will require blending
        ObjectMapBatch BlendObjects;
    };

    RenderBatchData GetRenderingBatch()
    {
        RenderBatchData renderBatch;

        for (auto &modelRenderer: Components::Get<ModelRenderer>())
        {
            if (!modelRenderer.GetModel())
            {
                continue;
            }

            bool hasTransparentMaterial = false;
            for (const auto &mesh: modelRenderer.GetModel()->GetMeshes())
            {
                if (mesh->GetMaterial() && mesh->GetMaterial()->GetRenderingMode() == MaterialRenderingMode::Transparent)
                {
                    hasTransparentMaterial = true;
                }
            }

            renderBatch.Objects[modelRenderer.GetModel()].push_back(&modelRenderer);

            if (hasTransparentMaterial)
            {
                renderBatch.BlendObjects[modelRenderer.GetModel()].push_back(&modelRenderer);
            }
        }

        return renderBatch;
    }

    void RenderBatch(const ObjectMapBatch &mapBatch, MaterialRenderingMode materialRenderingMode)
    {
        for (const auto &[model, renderers]: mapBatch)
        {
            for (auto mesh: model->GetMeshes())
            {
                if (mesh->GetMaterial() && mesh->GetMaterial()->GetRenderingMode() != materialRenderingMode)
                {
                    // We'll handle these afterward.
                    continue;
                }

                Renderer3D::PrepareMesh(mesh);

                bool hasStencilBufferOverride = false;

                for (auto renderer: renderers)
                {
                    renderer->GetParent()->GetTransform()->OnRender(0.f);

                    if (renderer->GetOverrideStencilBuffer())
                    {
                        hasStencilBufferOverride = true;
                        continue;
                    }

                    if (Renderer3D::AddInstance(renderer->GetParent()->GetTransform()->GetTransformationMatrix()))
                    {
                        Renderer3D::RenderMeshInstanced();
                    }
                }

                Renderer3D::RenderMeshInstanced();

                if (hasStencilBufferOverride)
                {
                    for (auto renderer: renderers)
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

    std::vector<Light *> GetLights()
    {
        std::vector<Light *> lights;

        for (auto &light: Components::Get<Light>())
        {
            lights.push_back(&light);
        }

        return lights;
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

void Pipeline3D::Run(const RenderingContext &context)
{
    Renderer3D::FrameReset();

    if (context.SceneCamera)
        Renderer3D::SetCamera(context.SceneCamera);

    Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);
    Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);

    // Prepare Data
    const auto renderingBatch = GetRenderingBatch();
    const auto lights = GetLights();

    // Upload Data
    for (auto light: lights)
    {
        Renderer3D::AddLight(light);
    }

    Renderer3D::UploadLights();

    // Render fully opaque objects.
    RenderBatch(renderingBatch.Objects, MaterialRenderingMode::Opaque);

    // Render objects which require discarding
    RenderBatch(renderingBatch.Objects, MaterialRenderingMode::Discard);

    // TODO: Render semi-transparent objects, we'll have to sort all objects by distance as well.

    // Sky box
    if (context.Skybox != nullptr)
    {
        Renderer::Skybox::Render(context.Skybox);
    }
}