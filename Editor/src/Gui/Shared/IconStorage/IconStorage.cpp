#include "IconStorage.hpp"

#include <imgui.h>

#include "Gui/Panels/AssetBrowser/AssetHierarchy/AssetHierarchy.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Math/ViewFit/ViewFit.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{

    enum class IconType
    {
        Static,
        Dynamic
    };

    struct Icon
    {
        Pine::UId Id;

        Pine::Asset *Asset = nullptr;

        Pine::Texture2D *StaticTexture = nullptr;
        Pine::Graphics::IFrameBuffer *DynamicTexture = nullptr;

        IconType Type = IconType::Static;

        bool m_Dirty = true;
    };

    std::unordered_map<Pine::UId, Icon> m_IconCache;

    Pine::Graphics::IFrameBuffer *m_PreviewFrameBuffer = nullptr;
    Pine::Graphics::IFrameBuffer *m_IconFrameBuffer = nullptr;

    Pine::Vector2f m_PreviewAngle = {0.f, 0.f};

    Pine::Texture2D *GetStaticIconFromAsset(Pine::Asset *asset)
    {
        switch (asset->GetType())
        {
            case Pine::AssetType::Texture2D:
                return dynamic_cast<Pine::Texture2D *>(asset);
            case Pine::AssetType::Tileset:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/tile-set");
            case Pine::AssetType::Tilemap:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/tile-map");
            case Pine::AssetType::Model:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/model");
            case Pine::AssetType::Level:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/level");
            case Pine::AssetType::Font:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/font");
            case Pine::AssetType::Shader:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/shader");
            case Pine::AssetType::Blueprint:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/blueprint");
            case Pine::AssetType::Material:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/material");
            case Pine::AssetType::CSharpScript:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/script");
            case Pine::AssetType::Audio:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/audio");
            default:
                return nullptr;
        }
    }

    bool ShouldGenerateDynamicIcon(const Pine::Asset *asset)
    {
        switch (asset->GetType())
        {
            case Pine::AssetType::Material:
                return true;
            case Pine::AssetType::Model:
                return true;
            default:
                return false;
        }
    }

    // Looking straight down an axis flattens a silhouette; a three-quarter view reads the shape.
    // Yaw/pitch in degrees, the same pair the preview drag offsets.
    constexpr Pine::Vector2f SubjectViewAngle = Pine::Vector2f(30.f, 20.f);

    // Leaves a little air around the subject instead of having it touch the edge of the icon.
    constexpr float SubjectViewPadding = 1.15f;

    // Key light sits over the camera's shoulder, so the subject is lit consistently no matter where
    // the view angle ends up.
    constexpr Pine::Vector2f KeyLightAngleOffset = Pine::Vector2f(25.f, 15.f);

    Pine::Vector3f DirectionFromAngles(const Pine::Vector2f &angles)
    {
        const float yaw = glm::radians(angles.x);
        const float pitch = glm::radians(glm::clamp(angles.y, -89.f, 89.f));

        return {
            glm::cos(pitch) * glm::sin(yaw),
            glm::sin(pitch),
            glm::cos(pitch) * glm::cos(yaw)
        };
    }

    // Aims a transform at a point from a given direction, i.e. the direction is where it sits
    // relative to the target, not where it looks.
    void PlaceLookingAt(Pine::Transform *transform, const Pine::Vector3f &target, const Pine::Vector3f &direction, float distance)
    {
        transform->SetLocalPosition(target + direction * distance);
        transform->SetLocalRotation(glm::quatLookAt(-direction, Pine::Vector3f(0.f, 1.f, 0.f)));
    }

    // The subject itself is never moved or scaled to fit - the camera is what moves. Scaling the
    // subject would misreport anything whose look depends on world scale, and would do nothing
    // about the depth range, which is the half that decides whether a very large or very small
    // model is visible at all.
    void SetupView(Pine::Camera *camera, Pine::Light *light, const Pine::Vector3f &boundsMin, const Pine::Vector3f &boundsMax, const Pine::Vector2f &viewAngle, float aspectRatio)
    {
        const auto fit = Pine::ViewFit::FromBounds(boundsMin, boundsMax, camera->GetFieldOfView(), aspectRatio, SubjectViewPadding);

        PlaceLookingAt(camera->GetParent()->GetTransform(), fit.Center, DirectionFromAngles(viewAngle), fit.Distance);

        camera->SetOverrideAspectRatio(aspectRatio);
        camera->SetNearPlane(fit.NearPlane);
        camera->SetFarPlane(fit.FarPlane);
        camera->OnRender(0.f);

        // Only the rotation of a directional light matters, but it still needs a sane position for
        // the transform to resolve.
        PlaceLookingAt(light->GetParent()->GetTransform(), fit.Center, DirectionFromAngles(viewAngle + KeyLightAngleOffset), fit.Distance);
    }

    Pine::Model *GetPreviewSphere()
    {
        static auto sphereModel = Pine::Assets::Get<Pine::Model>("editor/models/sphere");

        return sphereModel;
    }

    // What the icon is framed on: a material is previewed on the editor's sphere, a model on itself.
    bool GetSubjectBounds(const Icon &icon, Pine::Vector3f &boundsMin, Pine::Vector3f &boundsMax)
    {
        const Pine::Model *model = nullptr;

        if (icon.Asset->GetType() == Pine::AssetType::Material)
        {
            model = GetPreviewSphere();
        }
        else if (icon.Asset->GetType() == Pine::AssetType::Model)
        {
            model = dynamic_cast<Pine::Model *>(icon.Asset);
        }

        if (!model || model->GetMeshes().empty())
        {
            return false;
        }

        boundsMin = model->GetBoundingBoxMin();
        boundsMax = model->GetBoundingBoxMax();

        return true;
    }

    // Every subject renders at the origin with an identity transform, since the camera is what was
    // fitted to it.
    const Pine::Matrix4f &GetSubjectTransform()
    {
        static Pine::Entity *subjectEntity = nullptr;

        if (subjectEntity == nullptr)
        {
            subjectEntity = new Pine::Entity(Pine::UId::Empty());

            subjectEntity->AddComponent(new Pine::Transform());
            subjectEntity->GetTransform()->OnRender(0.f);
        }

        return subjectEntity->GetTransform()->GetTransformationMatrix();
    }

    void RenderMaterial(const Icon &icon)
    {
        const auto sphereModel = GetPreviewSphere();

        Pine::Renderer3D::PrepareMesh(sphereModel->GetMeshes()[0], dynamic_cast<Pine::Material *>(icon.Asset));
        Pine::Renderer3D::RenderMesh(GetSubjectTransform());
    }

    void RenderModel(const Icon &icon)
    {
        const auto model = dynamic_cast<Pine::Model *>(icon.Asset);

        for (const auto &mesh: model->GetMeshes())
        {
            Pine::Renderer3D::PrepareMesh(mesh);
            Pine::Renderer3D::RenderMesh(GetSubjectTransform());
        }
    }

    void GenerateDynamicTexture(const Icon &icon, bool isPreview)
    {
        if (icon.Type != IconType::Dynamic || icon.DynamicTexture == nullptr)
        {
            return;
        }

        static Pine::Entity* lightEntity = nullptr;
        static Pine::Entity* cameraEntity = nullptr;

        if (lightEntity == nullptr)
        {
            lightEntity = new Pine::Entity(Pine::UId::Empty());

            lightEntity->AddComponent(new Pine::Transform());
            lightEntity->AddComponent(new Pine::Light());

            lightEntity->GetComponent<Pine::Light>()->SetLightIntensity(2.f);
        }

        if (cameraEntity == nullptr)
        {
            cameraEntity = new Pine::Entity(Pine::UId::Empty());

            cameraEntity->AddComponent(new Pine::Transform());
            cameraEntity->AddComponent(new Pine::Camera());
        }

        Pine::Vector3f boundsMin;
        Pine::Vector3f boundsMax;

        if (!GetSubjectBounds(icon, boundsMin, boundsMax))
        {
            return;
        }

        const auto frameBuffer = isPreview ? m_PreviewFrameBuffer : m_IconFrameBuffer;
        const auto size = isPreview ? Pine::Vector2i(512, 512) : Pine::Vector2i(64, 64);

        // The drag orbits the camera rather than spinning the subject, so an off-origin model stays
        // framed instead of swinging out of view. Negated to keep the old feel, where the drag
        // turned the subject and not the camera.
        const auto viewAngle = isPreview
            ? SubjectViewAngle + Pine::Vector2f(m_PreviewAngle.x, -m_PreviewAngle.y)
            : SubjectViewAngle;

        SetupView(cameraEntity->GetComponent<Pine::Camera>(),
                  lightEntity->GetComponent<Pine::Light>(),
                  boundsMin,
                  boundsMax,
                  viewAngle,
                  static_cast<float>(size.x) / static_cast<float>(size.y));

        frameBuffer->Bind();

        Pine::Graphics::GetGraphicsAPI()->SetViewport(Pine::Vector2i(0), size);
        Pine::Graphics::GetGraphicsAPI()->ClearColor(Pine::Color(0, 0, 0, 0));
        Pine::Graphics::GetGraphicsAPI()->ClearBuffers(Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::DepthBuffer);
        Pine::Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);
        Pine::Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);

        Pine::Renderer3D::FrameReset();

        // Fill light, since a single directional light leaves the back of every subject black and
        // this pass cannot hold a second one (AddLight puts every directional in slot 0, and the
        // point/spot slots come from instance light indices that FrameReset zeroes).
        //
        // Deliberately over-bright, and not a physically meaningful ambient: this pass renders the
        // generic shader's linear HDR output straight into an LDR buffer and blits, so it never
        // gets the display transform - exposure, ACES and the linear->sRGB encode all live in the
        // post-process resolve. Values therefore land roughly a 2.2 gamma too dark and highlights
        // hard-clip. Tuned by eye against that, so it has to be retuned if this pass ever resolves
        // properly.
        Pine::Renderer3D::PrepareScene(Pine::Vector3f(2.0f, 2.0f, 2.0f), Pine::Vector4f(1.0f), 0.f, 0.f);
        Pine::Renderer3D::SetCamera(cameraEntity->GetComponent<Pine::Camera>());
        Pine::Renderer3D::AddLight(lightEntity->GetComponent<Pine::Light>());
        Pine::Renderer3D::UploadLights();

        if (icon.Asset->GetType() == Pine::AssetType::Material)
        {
            RenderMaterial(icon);
        }

        if (icon.Asset->GetType() == Pine::AssetType::Model)
        {
            RenderModel(icon);
        }

        if (!isPreview)
        {
            icon.DynamicTexture->Blit(
                frameBuffer,
                Pine::Graphics::ColorBuffer,
                Pine::Vector4i(0, 0, size.x, size.y),
                Pine::Vector4i(0, size.x, size.y, 0));
        }
    }

    void OnRender(Pine::RenderingContext*, Pine::RenderStage stage, float)
    {
        if (stage != Pine::RenderStage::PreRender)
            return;

        for (auto &[path, icon] : m_IconCache)
        {
            if (!icon.m_Dirty)
                continue;
            if (icon.Type == IconType::Static)
                continue;

            GenerateDynamicTexture(icon, false);

            icon.m_Dirty = false;
        }

        const auto& assets = Selection::GetSelectedAssets();
        if (!assets.empty())
        {
            auto asset = assets.front();

            if (ShouldGenerateDynamicIcon(asset) && m_IconCache.count(asset->GetUId()) > 0)
            {
                auto icon = m_IconCache[asset->GetUId()];

                GenerateDynamicTexture(icon, true);
            }
        }
    }
}

void Editor::Gui::IconStorage::Setup()
{
    m_PreviewFrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_PreviewFrameBuffer->Prepare();
    m_PreviewFrameBuffer->AttachTextures(512, 512, Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::Buffers::DepthBuffer);
    m_PreviewFrameBuffer->Finish();

    m_IconFrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_IconFrameBuffer->Prepare();
    m_IconFrameBuffer->AttachTextures(64, 64, Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::Buffers::DepthBuffer);
    m_IconFrameBuffer->Finish();

    Pine::RenderManager::AddRenderCallback(OnRender);
}

void Editor::Gui::IconStorage::Update()
{
    PINE_PF_SCOPE();

    std::vector<Pine::UId> removeList;

    // Find and remove unloaded assets from the icon cache
    for (auto& [iconAssetUId, icon] : m_IconCache)
    {
        auto asset = Pine::Assets::GetAssetByUId(iconAssetUId);

        if (asset)
        {
            continue;
        }

        if (icon.Type == IconType::Dynamic)
        {
            Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(icon.DynamicTexture);
            icon.DynamicTexture = nullptr;
        }

        removeList.push_back(iconAssetUId);
    }

    for (const auto &icon: removeList)
    {
        m_IconCache.erase(icon);
    }

    // Generate icons
    for (const auto &[id, asset]: Pine::Assets::GetAll())
    {
        Icon *icon = nullptr;

        icon = &m_IconCache[id];

        // If path is empty, it has just been created.
        if (icon->Id == Pine::UId::Empty())
        {
            icon->Id = id;
            icon->Asset = asset;
        }

        if (ShouldGenerateDynamicIcon(asset))
        {
            icon->Type = IconType::Dynamic;
        }

        icon->StaticTexture = GetStaticIconFromAsset(asset);
    }
}

Editor::Gui::AssetHierarchy::AssetIcon Editor::Gui::IconStorage::GetIconTexture(Pine::UId id)
{
    static auto invalidAssetIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file");

    if (!invalidAssetIcon)
    {
        return {};
    }

    if (!m_IconCache.count(id) || !m_IconCache[id].StaticTexture)
    {
        return
        {
            .IsDynamic = false,
            .DisplayIcon = invalidAssetIcon->GetGraphicsTexture(),
            .DisplayIconStatic = invalidAssetIcon->GetGraphicsTexture()
        };
    }

    auto& icon = m_IconCache[id];

    if (icon.Type == IconType::Dynamic)
    {
        if (icon.DynamicTexture == nullptr)
        {
            icon.DynamicTexture = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
            icon.DynamicTexture->Prepare();
            icon.DynamicTexture->AttachTextures(64, 64, Pine::Graphics::ColorBuffer, 0);
            icon.DynamicTexture->Finish();

            icon.m_Dirty = true;
        }

        return
        {
            .IsDynamic = true,
            .DisplayIcon = icon.DynamicTexture->GetColorBuffer(),
            .DisplayIconStatic = icon.StaticTexture->GetGraphicsTexture()
        };
    }

    return
    {
        .IsDynamic = false,
        .DisplayIcon = icon.StaticTexture->GetGraphicsTexture(),
        .DisplayIconStatic = icon.StaticTexture->GetGraphicsTexture()
    };
}

Pine::Graphics::ITexture* Editor::Gui::IconStorage::GetPreviewTexture()
{
    return m_PreviewFrameBuffer->GetColorBuffer();
}

// Slightly outside the scope for `IconStorage`, but this feels like an okay spot to put it.
void Editor::Gui::IconStorage::HandlePreviewDragging()
{
    static bool isDragging = false;
    static ImVec2 lastDragPos;

    const auto& io = ImGui::GetIO();

    if (ImGui::IsItemClicked())
    {
        isDragging = true;
        lastDragPos = io.MousePos;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        isDragging = false;
    }
    else if (isDragging)
    {
        auto delta = Pine::Vector2i(io.MousePos.x - lastDragPos.x, io.MousePos.y - lastDragPos.y);

        lastDragPos = io.MousePos;

        m_PreviewAngle += delta;
    }
}

void Editor::Gui::IconStorage::MarkIconDirty(Pine::UId id)
{
    if (!m_IconCache.count(id))
    {
        return;
    }

    m_IconCache[id].m_Dirty = true;
}

void Editor::Gui::IconStorage::Dispose()
{
    for (auto &[iconAssetPath, icon]: m_IconCache)
    {
        if (icon.Type == IconType::Dynamic && icon.DynamicTexture != nullptr)
        {
            Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(icon.DynamicTexture);
            icon.DynamicTexture = nullptr;
        }
    }

    Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_IconFrameBuffer);
    Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_PreviewFrameBuffer);

    m_PreviewFrameBuffer = nullptr;
    m_IconFrameBuffer = nullptr;
}
