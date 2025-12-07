#include "EntityUtilities.hpp"

#include <Pine/World/Components/ModelRenderer/ModelRenderer.hpp>
#include <Pine/World/Entity/Entity.hpp>

bool Pine::Utilities::Entity::UnpackModel(const ModelRenderer *modelRenderer)
{
    const auto model = modelRenderer->GetModel();

    if (model == nullptr)
    {
        // We need a model to unpack, probably programmer error.
        return false;
    }

    auto parentEntity = modelRenderer->GetParent();

    // Remove model renderer from parent (which currently renderers the full model)
    parentEntity->RemoveComponent(modelRenderer);

    // Create a new child for each mesh
    for (int i = 0; i < model->GetMeshes().size();i++)
    {
        const auto meshChild = parentEntity->CreateChild();
        const auto meshModelRenderer = meshChild->AddComponent<ModelRenderer>();

        meshChild->SetName("Mesh #" + std::to_string(i));

        meshModelRenderer->SetModel(model);
        meshModelRenderer->SetModelMeshIndex(i);
    }

    return true;
}