#include "Gizmo3D.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/Graphics/Graphics.hpp"

namespace
{

    void RenderIcon()
    {

    }

    // Writes the stencil buffer value of the selected objects to 0xFF, so we can outline them later.
    void PrepareSelectedObjects()
    {
        const auto& selectedEntities = Selection::GetSelectedEntities();

        if (selectedEntities.empty())
            return;

        for (auto& entity : selectedEntities)
        {
            auto modelRenderer = entity->GetComponent<Pine::ModelRenderer>();
            if (!modelRenderer || !modelRenderer->GetModel())
                continue;

            modelRenderer->SetOverrideStencilBuffer(true);
            modelRenderer->SetStencilBufferValue(0xFF);
        }
    }

    void RenderSelectedObjectsOutline()
    {
        const auto& selectedEntities = Selection::GetSelectedEntities();

        if (selectedEntities.empty())
            return;

        // Set stencil configuration so we only render outside of the stencil buffer
        Pine::Graphics::GetGraphicsAPI()->SetStencilFunction(Pine::Graphics::TestFunction::NotEqual, 0xFF, 0xFF);

        for (auto& entity : selectedEntities)
        {
            auto modelRenderer = entity->GetComponent<Pine::ModelRenderer>();
            if (!modelRenderer || !modelRenderer->GetModel())
                continue;

            // Restore these
            modelRenderer->SetOverrideStencilBuffer(false);
            modelRenderer->SetStencilBufferValue(0x00);

            // Render the model

        }


        Pine::Graphics::GetGraphicsAPI()->SetStencilFunction(Pine::Graphics::TestFunction::Always, 0x00, 0x00);
    }
}

void Gizmo::Gizmo3D::Setup()
{
}

void Gizmo::Gizmo3D::Render(Pine::Vector2f position)
{
}