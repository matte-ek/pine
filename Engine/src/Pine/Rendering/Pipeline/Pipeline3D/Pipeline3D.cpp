#include "Pipeline3D.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Light/Light.hpp"

namespace
{
    using namespace Pine;

    __inline bool IsComponentValid(const IComponent *component)
    {
        return component && component->GetActive() && component->GetParent()->GetActive();
    }

    std::unordered_map<Model *, std::vector<ModelRenderer *>> GetRenderingBatch()
    {
        std::unordered_map<Model *, std::vector<ModelRenderer *>> renderBatch;

        for (auto &modelRenderer : Components::Get<ModelRenderer>())
        {
            if (!IsComponentValid(&modelRenderer) && !modelRenderer.GetModel())
            {
                continue;
            }

            renderBatch[modelRenderer.GetModel()].push_back(&modelRenderer);
        }

        return renderBatch;
    }

    std::vector<Light *> GetLights()
    {
        std::vector<Light*> lights;

        for (auto &light : Components::Get<Light>())
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

void Pine::Pipeline3D::Setup()
{
}

void Pine::Pipeline3D::Shutdown()
{
}

void Pine::Pipeline3D::Run(RenderingContext &context)
{
    Renderer3D::SetCamera(context.SceneCamera);

    // Prepare Data
    const auto renderingBatch = GetRenderingBatch();
    const auto lights = GetLights();

    // Upload Data
    for (auto light : lights)
    {
    }

    // Rendering
    for (const auto& [model, renderers] : renderingBatch)
    {
        for (auto mesh : model->GetMeshes())
        {
            Renderer3D::PrepareMesh(mesh);

            for (auto renderer : renderers)
            {
                renderer->GetParent()->GetTransform()->OnRender(0.f);

                Renderer3D::RenderMesh(renderer->GetParent()->GetTransform()->GetTransformationMatrix());
            }
        }
    }

    // Skybox
}
