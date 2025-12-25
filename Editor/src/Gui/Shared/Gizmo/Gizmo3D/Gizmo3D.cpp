#include "Gizmo3D.hpp"

#include <imgui.h>

#include "IconsMaterialDesign.h"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Pine/Core/Log/Log.hpp"

namespace
{

    Pine::Shader* m_ObjectSolidShader3D = nullptr;

    void RenderIcon(Pine::Vector2f basePosition, Pine::Vector3f worldPosition, const Pine::Texture2D* texture)
    {
        if (!texture)
        {
            return;
        }

        constexpr float size = 40.f;

        const auto camera = RenderHandler::GetLevelRenderingContext()->SceneCamera;

        const Pine::Vector3f screenPosition = camera->WorldToScreenPoint(worldPosition);
        const Pine::Vector2f minPosition = { basePosition.x + screenPosition.x - size / 2.f, basePosition.y + screenPosition.y - size / 2.f };
        const Pine::Vector2f maxPosition = { basePosition.x + screenPosition.x + size - size / 2.f, basePosition.y + screenPosition.y + size - size / 2.f };

        if (screenPosition.z > 1.f)
        {
            return;
        }

        const std::uint64_t textureId = *static_cast<std::uint32_t*>(texture->GetGraphicsTexture()->GetGraphicsIdentifier());

        ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(textureId),
            {minPosition.x, minPosition.y},
            {maxPosition.x, maxPosition.y},
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            ImColor(0, 0, 0, 100)
            );

        ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(textureId),
            {minPosition.x - 1, minPosition.y - 1},
            {maxPosition.x - 1, maxPosition.y - 1},
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            ImColor(255, 255, 255)
            );
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

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = m_ObjectSolidShader3D;
        Pine::Renderer3D::GetRenderConfiguration().IgnoreShaderVersions = true;

        m_ObjectSolidShader3D->GetProgram()->Use();
        m_ObjectSolidShader3D->GetProgram()->GetUniformVariable("m_Color")->LoadVector3(Pine::Vector3f(0.8f, 0.3f, 0.f));

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
            const auto distance = glm::length(RenderHandler::GetLevelRenderingContext()->SceneCamera->GetParent()->GetTransform()->LocalPosition - entity->GetTransform()->LocalPosition);

            entity->GetTransform()->LocalScale += Pine::Vector3f((distance / 9.41f) * 0.05f);

            entity->GetTransform()->SetDirty();
            entity->GetTransform()->OnRender(0.f);

            entity->GetTransform()->LocalScale = oldScale;
            entity->GetTransform()->SetDirty();

            auto transformationMatrix = entity->GetTransform()->GetTransformationMatrix();

            int meshIndex = -1;
            for (const auto mesh : modelRenderer->GetModel()->GetMeshes())
            {
                meshIndex++;

                if (modelRenderer->GetModelMeshIndex() != -1 && modelRenderer->GetModelMeshIndex() != meshIndex)
                {
                    continue;
                }

                Pine::Renderer3D::PrepareMesh(mesh);
                Pine::Renderer3D::RenderMesh(transformationMatrix);
            }

            entity->GetTransform()->OnRender(0.f);
        }

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = nullptr;
        Pine::Renderer3D::GetRenderConfiguration().IgnoreShaderVersions = false;

        Pine::Graphics::GetGraphicsAPI()->SetStencilFunction(Pine::Graphics::TestFunction::Always, 0x00, 0x00);
    }

    void OnRender(const Pine::RenderingContext* context, const Pine::RenderStage stage, float)
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
    m_ObjectSolidShader3D = Pine::Assets::Get<Pine::Shader>("editor/shaders/generic-solid.shader");

    Pine::RenderManager::AddRenderCallback(OnRender);
}

void Gizmo::Gizmo3D::Render(Pine::Vector2f position)
{
    static auto lightGizmoIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/gizmo-light.png");
    static auto lightDirectionalGizmoIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/gizmo-light-directional.png");
    static auto cameraGizmoIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/gizmo-camera.png");

    if (Pine::RenderManager::GetCurrentRenderingContext() == nullptr)
    {
        return;
    }

    for (const auto& light : Pine::Components::Get<Pine::Light>())
    {
        if (Selection::IsSelected(light.GetParent()))
            continue;

        RenderIcon(position, light.GetParent()->GetTransform()->GetPosition(), light.GetLightType() == Pine::LightType::Directional ? lightDirectionalGizmoIcon : lightGizmoIcon);
    }

    for (auto& camera : Pine::Components::Get<Pine::Camera>())
    {
        if (Selection::IsSelected(camera.GetParent()))
            continue;
        if (&camera == RenderHandler::GetLevelRenderingContext()->SceneCamera)
            continue;

        // TODO: Actually render something nice if selected.
        /*
        const auto corners = camera.GetFrustumCorners();
        for (const auto& corner : corners)
        {
            auto screenSpace = RenderHandler::GetLevelRenderingContext()->SceneCamera->WorldToScreenPoint(corner);

            ImGui::GetWindowDrawList()->AddRectFilled({position.x + screenSpace.x, position.y + screenSpace.y}, {position.x + screenSpace.x + 2, position.y + screenSpace.y + 2}, ImColor(255, 255, 255));
        }
        */

        RenderIcon(position, camera.GetParent()->GetTransform()->GetPosition(), cameraGizmoIcon);
    }
}