#include "Gizmo3D.hpp"

#include <cmath>
#include <imgui.h>

#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Common/QuadTarget/QuadTarget.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Rendering/RenderHandler.hpp"

namespace
{
    Pine::Shader* m_ObjectSolidShader3D = nullptr;

    // Screen-space outline resources. The silhouette of the selected objects is
    // rendered flat white into m_OutlineMaskBuffer, then a full-screen pass
    // (m_OutlineShader) edge-detects that mask and paints a constant-width halo
    // over the scene. Doing it in screen space keeps the outline exactly N
    // pixels thick on every model, regardless of shape, scale or distance.
    Pine::Shader* m_OutlineShader = nullptr;
    Pine::Graphics::IFrameBuffer* m_OutlineMaskBuffer = nullptr;

    // Outline appearance.
    constexpr int OUTLINE_WIDTH = 3; // thickness in pixels (must be <= MAX_RADIUS in the shader)
    const Pine::Vector3f OUTLINE_COLOR = Pine::Vector3f(0.9f, 0.4f, 0.05f);

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

        ImGui::GetWindowDrawList()->AddImage(textureId,
            {minPosition.x, minPosition.y},
            {maxPosition.x, maxPosition.y},
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            ImColor(0, 0, 0, 100)
            );

        ImGui::GetWindowDrawList()->AddImage(textureId,
            {minPosition.x - 1, minPosition.y - 1},
            {maxPosition.x - 1, maxPosition.y - 1},
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            ImColor(255, 255, 255)
            );
    }

    // Renders the selected objects' silhouette (flat white) into m_OutlineMaskBuffer.
    void RenderOutlineMask(const Pine::RenderingContext* context, const std::vector<Pine::Entity*>& selectedEntities)
    {
        auto graphics = Pine::Graphics::GetGraphicsAPI();

        m_OutlineMaskBuffer->Bind();

        graphics->SetViewport(Pine::Vector2i(0), context->Size);
        graphics->ClearColor(Pine::Color(0, 0, 0, 0));
        graphics->ClearBuffers(Pine::Graphics::ColorBuffer);

        // We want the full silhouette regardless of self-occlusion or scene geometry
        // in front of the object, so no depth/stencil testing here.
        graphics->SetDepthTestEnabled(false);
        graphics->SetStencilTestEnabled(false);
        graphics->SetBlendingEnabled(false);

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = m_ObjectSolidShader3D;
        Pine::Renderer3D::GetRenderConfiguration().IgnoreShaderVersions = true;
        Pine::Renderer3D::GetRenderConfiguration().SkipMaterialInitialization = true;

        m_ObjectSolidShader3D->GetProgram()->Use();
        m_ObjectSolidShader3D->GetProgram()->GetUniformVariable("m_Color")->LoadVector3(Pine::Vector3f(1.f));

        for (auto& entity : selectedEntities)
        {
            auto modelRenderer = entity->GetComponent<Pine::ModelRenderer>();
            if (!modelRenderer || !modelRenderer->GetModel())
                continue;

            const auto transformationMatrix = entity->GetTransform()->GetTransformationMatrix();

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
        }

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = nullptr;
        Pine::Renderer3D::GetRenderConfiguration().IgnoreShaderVersions = false;
        Pine::Renderer3D::GetRenderConfiguration().SkipMaterialInitialization = false;
    }

    void RenderSelectedObjectsOutline(const Pine::RenderingContext* context)
    {
        const auto& selectedEntities = Selection::GetSelectedEntities();

        if (selectedEntities.empty())
            return;

        auto graphics = Pine::Graphics::GetGraphicsAPI();

        // Pass 1: render the silhouette of the selected objects into the mask buffer.
        RenderOutlineMask(context, selectedEntities);

        // Pass 2: edge-detect the mask and composite the outline back onto the scene.
        Pine::RenderManager::GetInternalFrameBuffer()->Bind();

        graphics->SetViewport(Pine::Vector2i(0), context->Size);
        graphics->SetDepthTestEnabled(false);
        graphics->SetBlendingEnabled(true);
        graphics->SetBlendingFunction(Pine::Graphics::BlendingFunction::SourceAlpha, Pine::Graphics::BlendingFunction::OneMinusSourceAlpha);

        auto program = m_OutlineShader->GetProgram();

        program->Use();
        m_OutlineMaskBuffer->GetColorBuffer()->Bind(0);

        program->GetUniformVariable("outlineColor")->LoadVector3(OUTLINE_COLOR);
        program->GetUniformVariable("outlineWidth")->LoadInteger(OUTLINE_WIDTH);
        program->GetUniformVariable("bufferSize")->LoadVector2(Pine::Vector2f(m_OutlineMaskBuffer->GetSize().x, m_OutlineMaskBuffer->GetSize().y));

        Pine::Rendering::Common::QuadTarget::Render();

        // Restore state for anything that renders after us (e.g. collider wireframes).
        graphics->SetBlendingEnabled(false);
        graphics->SetDepthTestEnabled(true);
    }

    // ------- Collider previews (ImGui overlay) -------
    //
    // Collider wireframes are drawn as screen-space lines via ImGui's draw list, the same
    // way the gizmo icons are: world-space shape points are projected with the scene camera
    // and connected with lines. Thickness is therefore uniform (in pixels), and shapes a
    // scaled primitive mesh can't express (capsules) come for free. Like the previous
    // approach this is an always-on-top overlay (no depth occlusion).

    constexpr ImU32 ColliderLineColor = IM_COL32(60, 205, 90, 230);
    constexpr float ColliderLineThickness = 1.5f;
    constexpr int ColliderCircleSegments = 32;
    constexpr float ColliderPi = 3.14159265358979323846f;

    struct ColliderGizmoRenderer
    {
        ImDrawList* DrawList;
        const Pine::Camera* Cam;
        Pine::Vector2f Base;

        // Projects a world point to viewport pixels; returns false if it's behind the camera.
        bool Project(const Pine::Vector3f& world, ImVec2& out) const
        {
            const Pine::Vector3f screen = Cam->WorldToScreenPoint(world);
            if (screen.z > 1.f)
                return false;

            out = ImVec2(Base.x + screen.x, Base.y + screen.y);
            return true;
        }

        void Line(const Pine::Vector3f& a, const Pine::Vector3f& b) const
        {
            ImVec2 pa, pb;

            // Skip the whole edge if either end is behind the camera (avoids garbage projection).
            if (Project(a, pa) && Project(b, pb))
            {
                DrawList->AddLine(pa, pb, ColliderLineColor, ColliderLineThickness);
            }
        }

        // Full circle in the plane spanned by axisU/axisV (both radius-length and perpendicular).
        void Circle(const Pine::Vector3f& center, const Pine::Vector3f& axisU, const Pine::Vector3f& axisV) const
        {
            Pine::Vector3f prev = center + axisU;

            for (int i = 1; i <= ColliderCircleSegments; i++)
            {
                const float angle = (static_cast<float>(i) / ColliderCircleSegments) * 2.f * ColliderPi;
                const Pine::Vector3f cur = center + axisU * std::cos(angle) + axisV * std::sin(angle);

                Line(prev, cur);
                prev = cur;
            }
        }

        // Half circle from center+axisU, bulging through center+axisV, to center-axisU.
        void Arc(const Pine::Vector3f& center, const Pine::Vector3f& axisU, const Pine::Vector3f& axisV) const
        {
            Pine::Vector3f prev = center + axisU;

            for (int i = 1; i <= ColliderCircleSegments / 2; i++)
            {
                const float angle = (static_cast<float>(i) / (ColliderCircleSegments / 2)) * ColliderPi;
                const Pine::Vector3f cur = center + axisU * std::cos(angle) + axisV * std::sin(angle);

                Line(prev, cur);
                prev = cur;
            }
        }
    };

    void DrawBoxCollider(const ColliderGizmoRenderer& r, const Pine::Vector3f& center, const glm::quat& rotation, const Pine::Vector3f& halfExtents)
    {
        Pine::Vector3f corners[8];

        for (int i = 0; i < 8; i++)
        {
            const Pine::Vector3f local(
                (i & 1) ? halfExtents.x : -halfExtents.x,
                (i & 2) ? halfExtents.y : -halfExtents.y,
                (i & 4) ? halfExtents.z : -halfExtents.z);

            corners[i] = center + rotation * local;
        }

        static constexpr int edges[12][2] =
        {
            {0,1},{1,3},{3,2},{2,0}, // -z face
            {4,5},{5,7},{7,6},{6,4}, // +z face
            {0,4},{1,5},{2,6},{3,7}  // connecting edges
        };

        for (const auto& edge : edges)
        {
            r.Line(corners[edge[0]], corners[edge[1]]);
        }
    }

    void DrawSphereCollider(const ColliderGizmoRenderer& r, const Pine::Vector3f& center, const float radius)
    {
        // Three world-axis great circles.
        r.Circle(center, Pine::Vector3f(radius, 0, 0), Pine::Vector3f(0, radius, 0));
        r.Circle(center, Pine::Vector3f(radius, 0, 0), Pine::Vector3f(0, 0, radius));
        r.Circle(center, Pine::Vector3f(0, radius, 0), Pine::Vector3f(0, 0, radius));
    }

    void DrawCapsuleCollider(const ColliderGizmoRenderer& r, const Pine::Vector3f& center, const glm::quat& rotation, const float radius, const float halfHeight)
    {
        // PhysX capsules are aligned along their local X axis.
        const Pine::Vector3f axis = rotation * Pine::Vector3f(1, 0, 0);
        const Pine::Vector3f up = rotation * Pine::Vector3f(0, 1, 0);
        const Pine::Vector3f forward = rotation * Pine::Vector3f(0, 0, 1);

        const Pine::Vector3f capA = center + axis * halfHeight;
        const Pine::Vector3f capB = center - axis * halfHeight;

        const Pine::Vector3f rUp = up * radius;
        const Pine::Vector3f rForward = forward * radius;
        const Pine::Vector3f rAxis = axis * radius;

        // Rings around each cap center (perpendicular to the axis).
        r.Circle(capA, rUp, rForward);
        r.Circle(capB, rUp, rForward);

        // Hemisphere caps, bulging outward along the axis.
        r.Arc(capA, rUp, rAxis);
        r.Arc(capA, rForward, rAxis);
        r.Arc(capB, rUp, -rAxis);
        r.Arc(capB, rForward, -rAxis);

        // Cylinder side lines connecting the two rings.
        r.Line(capA + rUp, capB + rUp);
        r.Line(capA - rUp, capB - rUp);
        r.Line(capA + rForward, capB + rForward);
        r.Line(capA - rForward, capB - rForward);
    }

    void RenderColliders(const Pine::Vector2f basePosition)
    {
        const auto& selectedEntities = Selection::GetSelectedEntities();

        if (selectedEntities.empty())
            return;

        const auto camera = Editor::RenderHandler::GetLevelRenderingContext()->SceneCamera;

        if (!camera)
            return;

        const ColliderGizmoRenderer renderer { ImGui::GetWindowDrawList(), camera, basePosition };

        for (auto& entity : selectedEntities)
        {
            const auto collider = entity->GetComponent<Pine::Collider>();

            if (!collider)
                continue;

            const auto transform = entity->GetTransform();
            const glm::quat rotation = transform->GetRotation();

            // 'size' matches the PhysX geometry: box half-extents, sphere/capsule radius in x,
            // capsule half-height in y (see Collider::CreateCollisionShape).
            const Pine::Vector3f center = transform->GetPosition() + collider->GetPosition();
            const Pine::Vector3f size = collider->GetSize() * transform->GetScale();

            switch (collider->GetColliderType())
            {
                case Pine::ColliderType::Box:
                    DrawBoxCollider(renderer, center, rotation, size);
                    break;
                case Pine::ColliderType::Sphere:
                    DrawSphereCollider(renderer, center, size.x);
                    break;
                case Pine::ColliderType::Capsule:
                    DrawCapsuleCollider(renderer, center, rotation, size.x, size.y);
                    break;
                case Pine::ColliderType::ConvexMesh:
                case Pine::ColliderType::ConcaveMesh:
                case Pine::ColliderType::HeightField:
                    break;
            }
        }
    }

    void OnRender(const Pine::RenderingContext* context, const Pine::RenderStage stage, float)
    {
        if (context != Editor::RenderHandler::GetLevelRenderingContext())
        {
            return;
        }

        if (stage == Pine::RenderStage::PostRender3D)
        {
            RenderSelectedObjectsOutline(context);

            return;
        }
    }
}

void Editor::Gui::Gizmo::Gizmo3D::Setup()
{
    m_ObjectSolidShader3D = Pine::Assets::Get<Pine::Shader>("editor/shaders/generic-solid");
    m_OutlineShader = Pine::Assets::Get<Pine::Shader>("editor/shaders/outline");

    // Silhouette mask for the selection outline. Sized to the internal render
    // resolution so its pixel space lines up with the scene framebuffer.
    m_OutlineMaskBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_OutlineMaskBuffer->Prepare();
    m_OutlineMaskBuffer->AttachTextures(
        Pine::Renderer3D::Specifications::General::INTERNAL_WIDTH,
        Pine::Renderer3D::Specifications::General::INTERNAL_HEIGHT,
        Pine::Graphics::ColorBuffer);
    m_OutlineMaskBuffer->Finish();

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

    RenderColliders(position);

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