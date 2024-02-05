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

    Pine::Shader* m_ObjectSolidShader = nullptr;

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

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = m_ObjectSolidShader;

        m_ObjectSolidShader->GetProgram()->Use();
        m_ObjectSolidShader->GetProgram()->GetUniformVariable("m_Color")->LoadVector3(Pine::Vector3f(1.f, 0.f, 0.f));

        for (auto& entity : selectedEntities)
        {
            auto modelRenderer = entity->GetComponent<Pine::ModelRenderer>();
            if (!modelRenderer || !modelRenderer->GetModel())
                continue;

            // Restore these
            modelRenderer->SetOverrideStencilBuffer(false);
            modelRenderer->SetStencilBufferValue(0x00);

            // Render the model
            auto oldScale = entity->GetTransform()->LocalScale;

            entity->GetTransform()->LocalScale += Pine::Vector3f(0.30f);
            entity->GetTransform()->OnRender(0.f);
            entity->GetTransform()->LocalScale = oldScale;

            auto transformationMatrix = entity->GetTransform()->GetTransformationMatrix();

            for (auto mesh : modelRenderer->GetModel()->GetMeshes())
            {
                Pine::Renderer3D::PrepareMesh(mesh);
                Pine::Renderer3D::RenderMesh(transformationMatrix);
            }

            entity->GetTransform()->OnRender(0.f);
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
    m_ObjectSolidShader = Pine::Assets::Get<Pine::Shader>("editor/shaders/generic-solid.shader");

    Pine::RenderManager::AddRenderCallback(OnRender);
}

void Gizmo::Gizmo3D::Render(Pine::Vector2f position)
{
}