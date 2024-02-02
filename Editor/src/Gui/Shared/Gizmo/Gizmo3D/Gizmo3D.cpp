#include "Gizmo3D.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Rendering/RenderHandler.hpp"

namespace
{

    Pine::Shader* m_ObjectOutlineShader = nullptr;

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

        // Set stencil configuration, so we only render outside the stencil buffer
        Pine::Graphics::GetGraphicsAPI()->SetStencilFunction(Pine::Graphics::TestFunction::NotEqual, 0xFF, 0xFF);

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = m_ObjectOutlineShader;

        for (auto& entity : selectedEntities)
        {
            auto modelRenderer = entity->GetComponent<Pine::ModelRenderer>();
            if (!modelRenderer || !modelRenderer->GetModel())
                continue;

            // Restore these
            modelRenderer->SetOverrideStencilBuffer(false);
            modelRenderer->SetStencilBufferValue(0x00);

            // Render the model
            auto transformationMatrix = entity->GetTransform()->GetTransformationMatrix();

            transformationMatrix = glm::scale(transformationMatrix, Pine::Vector3f(1.05f));

            for (auto mesh : modelRenderer->GetModel()->GetMeshes())
            {
                Pine::Renderer3D::PrepareMesh(mesh);
                Pine::Renderer3D::RenderMesh(transformationMatrix);
            }
        }

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = nullptr;

        Pine::Graphics::GetGraphicsAPI()->SetStencilFunction(Pine::Graphics::TestFunction::Always, 0x00, 0x00);
    }

    void OnRender(Pine::RenderingContext* context, Pine::RenderStage stage, float)
    {
        if (context != RenderHandler::GetLevelRenderingContext())
        {
            return;
        }

        if (stage == Pine::RenderStage::PreRender3D)
        {
            PrepareSelectedObjects();

            return;
        }

        if (stage == Pine::RenderStage::PostRender3D)
        {
            RenderSelectedObjectsOutline();

            return;
        }
    }
}

void Gizmo::Gizmo3D::Setup()
{
    m_ObjectOutlineShader = Pine::Assets::Get<Pine::Shader>("editor/shaders/generic-selected.shader");

    Pine::RenderManager::AddRenderCallback(OnRender);
}

void Gizmo::Gizmo3D::Render(Pine::Vector2f position)
{
}