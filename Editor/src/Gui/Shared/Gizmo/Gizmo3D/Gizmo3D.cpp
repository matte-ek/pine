#include "Gizmo3D.hpp"

#include <imgui.h>

#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Rendering/RenderHandler.hpp"

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

        const auto camera = Editor::RenderHandler::GetLevelRenderingContext()->SceneCamera;

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
        Pine::Renderer3D::GetRenderConfiguration().SkipMaterialInitialization = true;

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
            auto oldScale = entity->GetTransform()->GetLocalScale();
            const auto distance = glm::length(Editor::RenderHandler::GetLevelRenderingContext()->SceneCamera->GetParent()->GetTransform()->GetLocalPosition() - entity->GetTransform()->GetLocalPosition());

            entity->GetTransform()->SetLocalScale(entity->GetTransform()->GetLocalScale() + Pine::Vector3f((distance / 9.41f) * 0.05f));

            entity->GetTransform()->SetDirty();
            entity->GetTransform()->OnRender(0.f);

            entity->GetTransform()->SetLocalScale(oldScale);
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
        Pine::Renderer3D::GetRenderConfiguration().SkipMaterialInitialization = false;

        Pine::Graphics::GetGraphicsAPI()->SetStencilFunction(Pine::Graphics::TestFunction::Always, 0x00, 0x00);
    }

    void RenderCollider()
    {
        static auto boxPrimitiveModel = Pine::Assets::Get<Pine::Model>("engine/primitive/cube");
        static auto spherePrimitiveModel = Pine::Assets::Get<Pine::Model>("engine/primitive/sphere");
        static auto capsulePrimitiveModel = Pine::Assets::Get<Pine::Model>("engine/primitive/capsule");

        const auto& selectedEntities = Selection::GetSelectedEntities();

        if (selectedEntities.empty())
            return;

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = m_ObjectSolidShader3D;
        Pine::Renderer3D::GetRenderConfiguration().IgnoreShaderVersions = true;
        Pine::Renderer3D::GetRenderConfiguration().SkipMaterialInitialization = true;

        Pine::Graphics::GetGraphicsAPI()->SetDepthTestEnabled(false);
        Pine::Graphics::GetGraphicsAPI()->SetWireframeEnabled(true);

        m_ObjectSolidShader3D->GetProgram()->Use();
        m_ObjectSolidShader3D->GetProgram()->GetUniformVariable("m_Color")->LoadVector3(Pine::Vector3f(0.0f, 0.8f, 0.2f));

        for (auto& entity : selectedEntities)
        {
            auto collider = entity->GetComponent<Pine::Collider>();

            if (!collider)
            {
                continue;
            }

            Pine::Model* model = nullptr;
            Pine::Vector3f size = collider->GetSize();

            switch (collider->GetColliderType())
            {
                case Pine::ColliderType::Box:
                    model = boxPrimitiveModel;
                    break;
                case Pine::ColliderType::Sphere:
                    model = spherePrimitiveModel;
                    size = Pine::Vector3f(collider->GetSize().x);
                    break;
                case Pine::ColliderType::Capsule:
                    //model = capsulePrimitiveModel;
                    //size = Pine::Vector3f(collider->GetSize().x, collider->GetSize().y, 1.f);
                    break;
                case Pine::ColliderType::ConvexMesh:
                case Pine::ColliderType::ConcaveMesh:
                case Pine::ColliderType::HeightField:
                    break;
            }

            if (!model)
            {
                continue;
            }

            auto parentTransform = entity->GetTransform();
            auto transformationMatrix = Pine::Matrix4f(1.f);

            transformationMatrix = glm::translate(transformationMatrix, parentTransform->GetPosition() + collider->GetPosition());
            transformationMatrix *= glm::toMat4(parentTransform->GetRotation());
            transformationMatrix = glm::scale(transformationMatrix, parentTransform->GetScale() * size);

            Pine::Renderer3D::PrepareMesh(model->GetMeshes()[0]);
            Pine::Renderer3D::RenderMesh(transformationMatrix);
        }

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = nullptr;
        Pine::Renderer3D::GetRenderConfiguration().IgnoreShaderVersions = false;
        Pine::Renderer3D::GetRenderConfiguration().SkipMaterialInitialization = false;

        Pine::Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);
        Pine::Graphics::GetGraphicsAPI()->SetWireframeEnabled(false);
    }

    void OnRender(const Pine::RenderingContext* context, const Pine::RenderStage stage, float)
    {
        if (context != Editor::RenderHandler::GetLevelRenderingContext())
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
            RenderCollider();

            return;
        }
    }
}

void Editor::Gui::Gizmo::Gizmo3D::Setup()
{
    m_ObjectSolidShader3D = Pine::Assets::Get<Pine::Shader>("editor/shaders/generic-solid");

    Pine::RenderManager::AddRenderCallback(OnRender);
}

void Editor::Gui::Gizmo::Gizmo3D::Render(Pine::Vector2f position)
{
    static auto lightGizmoIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/gizmo-light");
    static auto lightDirectionalGizmoIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/gizmo-light-directional");
    static auto cameraGizmoIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/gizmo-camera");

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