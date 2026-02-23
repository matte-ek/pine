#include "IconStorage.hpp"

#include <imgui.h>

#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Model/Model.hpp"
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

    void RenderMaterial(const Icon &icon, bool isPreview)
    {
        static auto sphereModel = Pine::Assets::Get<Pine::Model>("editor/models/sphere");
        static Pine::Entity* sphereEntity = nullptr;

        if (sphereEntity == nullptr)
        {
            sphereEntity = new Pine::Entity(0);
            sphereEntity->AddComponent(new Pine::Transform());
            sphereEntity->GetTransform()->SetLocalPosition(Pine::Vector3f(0, 0, -0.2f));
            sphereEntity->GetTransform()->SetLocalScale(Pine::Vector3f(10.f));
        }

        if (isPreview)
        {
            sphereEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(m_PreviewAngle.y, -m_PreviewAngle.x, 0.f));
        }
        else
        {
            sphereEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(0.f, -90.f, 0.f));
        }

        sphereEntity->GetTransform()->OnRender(0.f);

        Pine::Renderer3D::PrepareMesh(sphereModel->GetMeshes()[0], dynamic_cast<Pine::Material*>(icon.Asset));
        Pine::Renderer3D::RenderMesh(sphereEntity->GetTransform()->GetTransformationMatrix());
    }

    void RenderModel(const Icon& icon, bool isPreview)
    {
        static Pine::Entity* modelEntity = nullptr;

        auto model = dynamic_cast<Pine::Model*>(icon.Asset);
        if (!model)
        {
            return;
        }

        if (modelEntity == nullptr)
        {
            modelEntity = new Pine::Entity(0);
            modelEntity->AddComponent(new Pine::Transform());
        }

        Pine::Vector3f globalMins {};
        Pine::Vector3f globalMaxs {};

        for (const auto& mesh : model->GetMeshes())
        {
            globalMins = glm::min(globalMins, mesh->GetBoundingBoxMin());
            globalMaxs = glm::max(globalMaxs, mesh->GetBoundingBoxMax());
        }

        const auto center = (globalMins + globalMaxs) * 0.5f;
        const auto size = (glm::abs(globalMins) + globalMaxs);

        modelEntity->GetTransform()->SetLocalScale(Pine::Vector3f(1.f));
        modelEntity->GetTransform()->SetLocalPosition(Pine::Vector3f(0, -center.y, -1.f));

        if (isPreview)
        {
            modelEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(m_PreviewAngle.y, -m_PreviewAngle.x, 0.f));
        }
        else
        {
            modelEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(0.f));
        }

        modelEntity->GetTransform()->OnRender(0.f);

        for (const auto& mesh : model->GetMeshes())
        {
            Pine::Renderer3D::PrepareMesh(mesh);
            Pine::Renderer3D::RenderMesh(modelEntity->GetTransform()->GetTransformationMatrix());
        }
    }

    void GenerateDynamicTexture(const Icon &icon, bool isPreview)
    {
        if (icon.Type != IconType::Dynamic)
        {
            return;
        }

        static Pine::Entity* lightEntity = nullptr;
        static Pine::Entity* cameraEntity = nullptr;

        if (lightEntity == nullptr)
        {
            lightEntity = new Pine::Entity(0);

            lightEntity->AddComponent(new Pine::Transform());
            lightEntity->AddComponent(new Pine::Light());

            lightEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(0.f, 0.f, 0.f));
        }

        if (cameraEntity == nullptr)
        {
            cameraEntity = new Pine::Entity(0);

            cameraEntity->AddComponent(new Pine::Transform());
            cameraEntity->AddComponent(new Pine::Camera());

            cameraEntity->GetComponent<Pine::Camera>()->SetOverrideAspectRatio(1.f);
            cameraEntity->GetComponent<Pine::Camera>()->OnRender(0.f);
        }

        auto frameBuffer = isPreview ? m_PreviewFrameBuffer : m_IconFrameBuffer;
        auto size = isPreview ? Pine::Vector2i(512, 512) : Pine::Vector2i(64, 64);

        frameBuffer->Bind();

        Pine::Graphics::GetGraphicsAPI()->SetViewport(Pine::Vector2i(0), size);
        Pine::Graphics::GetGraphicsAPI()->ClearColor(Pine::Color(0, 0, 0, 0));
        Pine::Graphics::GetGraphicsAPI()->ClearBuffers(Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::DepthBuffer);
        Pine::Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);
        Pine::Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);

        Pine::Renderer3D::FrameReset();

        Pine::Renderer3D::SetCamera(cameraEntity->GetComponent<Pine::Camera>());
        Pine::Renderer3D::AddLight(lightEntity->GetComponent<Pine::Light>());
        Pine::Renderer3D::UploadLights();

        if (icon.Asset->GetType() == Pine::AssetType::Material)
        {
            RenderMaterial(icon, isPreview);
        }

        if (icon.Asset->GetType() == Pine::AssetType::Model)
        {
            RenderModel(icon, isPreview);
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
    m_IconFrameBuffer->AttachTextures(64, 64, Pine::Graphics::Buffers::ColorBuffer);
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

            if (icon->DynamicTexture == nullptr)
            {
                icon->DynamicTexture = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();

                icon->DynamicTexture->Prepare();
                icon->DynamicTexture->AttachTextures(64, 64, Pine::Graphics::ColorBuffer, 0);
                icon->DynamicTexture->Finish();
            }

            icon->m_Dirty = true;
        }

        icon->StaticTexture = GetStaticIconFromAsset(asset);
    }
}

Pine::Graphics::ITexture* Editor::Gui::IconStorage::GetIconTexture(Pine::UId id)
{
    static auto invalidAssetIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file");

    if (!m_IconCache.count(id) || !m_IconCache[id].StaticTexture)
    {
        return invalidAssetIcon->GetGraphicsTexture();
    }

    const auto& icon = m_IconCache[id];

    if (icon.Type == IconType::Dynamic)
    {
        return icon.DynamicTexture->GetColorBuffer();
    }

    return m_IconCache[id].StaticTexture->GetGraphicsTexture();
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
