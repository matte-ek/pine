#include "IconStorage.hpp"

#include <imgui.h>

#include "Gui/Panels/AssetBrowser/AssetHierarchy/AssetHierarchy.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Rendering/AssetPreview/AssetPreview.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

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

    // The dirty icons and the large preview both come out of the shared asset preview renderer, so
    // what the browser shows and what the debug server serves cannot drift apart.
    void GenerateDynamicTexture(const Icon &icon, bool isPreview)
    {
        if (icon.Type != IconType::Dynamic || icon.DynamicTexture == nullptr)
        {
            return;
        }

        const auto frameBuffer = isPreview ? m_PreviewFrameBuffer : m_IconFrameBuffer;
        const auto size = isPreview ? Pine::Vector2i(512, 512) : Pine::Vector2i(64, 64);

        // The drag orbits the camera rather than spinning the subject, so an off-origin model stays
        // framed instead of swinging out of view. Negated to keep the old feel, where the drag
        // turned the subject and not the camera.
        Editor::AssetPreview::Options options;

        options.Size = size;
        options.ViewAngle = isPreview
            ? Editor::AssetPreview::DefaultViewAngle + Pine::Vector2f(m_PreviewAngle.x, -m_PreviewAngle.y)
            : Editor::AssetPreview::DefaultViewAngle;

        if (!Editor::AssetPreview::Render(icon.Asset, options, frameBuffer))
        {
            return;
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

            if (Editor::AssetPreview::Supports(asset) && m_IconCache.count(asset->GetUId()) > 0)
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

        if (Editor::AssetPreview::Supports(asset))
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
