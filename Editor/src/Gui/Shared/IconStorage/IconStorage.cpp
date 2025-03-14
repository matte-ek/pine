#include "IconStorage.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/Assets/Model/Model.hpp"
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
        std::string Path;

        Pine::IAsset *Asset = nullptr;

        Pine::Graphics::IFrameBuffer *FrameBuffer = nullptr;
        Pine::Texture2D *Texture = nullptr;

        IconType Type = IconType::Static;

        bool m_Dirty = true;
    };

    Pine::Texture2D *GetStaticIconFromAsset(Pine::IAsset *asset)
    {
        switch (asset->GetType())
        {
            case Pine::AssetType::Texture2D:
                return dynamic_cast<Pine::Texture2D *>(asset);
            case Pine::AssetType::Tileset:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/tile-set.png");
            case Pine::AssetType::Tilemap:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/tile-map.png");
            case Pine::AssetType::Model:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/model.png");
            case Pine::AssetType::Level:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/level.png");
            case Pine::AssetType::Font:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/font.png");
            case Pine::AssetType::Shader:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/shader.png");
            case Pine::AssetType::Blueprint:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/blueprint.png");
            case Pine::AssetType::Material:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/material.png");
            case Pine::AssetType::CSharpScript:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/script.png");
            case Pine::AssetType::Audio:
                return Pine::Assets::Get<Pine::Texture2D>("editor/icons/audio.png");
            default:
                return nullptr;
        }
    }

    bool ShouldGenerateDynamicIcon(Pine::IAsset *asset)
    {
        switch (asset->GetType())
        {
            case Pine::AssetType::Material:
                return true;
            default:
                return false;
        }
    }

    void RenderMaterial(Icon &icon)
    {
        static auto sphereModel = Pine::Assets::Get<Pine::Model>("editor/models/sphere.fbx");
        static Pine::Entity* sphereEntity = nullptr;

        if (sphereEntity == nullptr)
        {
            sphereEntity = new Pine::Entity(0);
            sphereEntity->AddComponent(new Pine::Transform());

            sphereEntity->GetTransform()->LocalScale = Pine::Vector3f(10.f);
            sphereEntity->GetTransform()->LocalPosition = Pine::Vector3f(0, 0, -0.2f);
            sphereEntity->GetTransform()->OnRender(0.f);
        }

        Pine::Renderer3D::PrepareMesh(sphereModel->GetMeshes()[0], dynamic_cast<Pine::Material*>(icon.Asset));
        Pine::Renderer3D::RenderMesh(sphereEntity->GetTransform()->GetTransformationMatrix());
    }

    void GenerateDynamicIcon(Icon &icon)
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

            lightEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(0.f, -180.f, 0.f));
        }

        if (cameraEntity == nullptr)
        {
            cameraEntity = new Pine::Entity(0);
            cameraEntity->AddComponent(new Pine::Transform());
            cameraEntity->AddComponent(new Pine::Camera());
            cameraEntity->GetComponent<Pine::Camera>()->SetOverrideAspectRatio(1.f);
            cameraEntity->GetComponent<Pine::Camera>()->OnRender(0.f);
        }

        icon.FrameBuffer->Bind();

        Pine::Graphics::GetGraphicsAPI()->SetViewport(Pine::Vector2i(0), Pine::Vector2i(128, 128));
        Pine::Graphics::GetGraphicsAPI()->ClearColor(Pine::Color(0, 0, 0, 0));
        Pine::Graphics::GetGraphicsAPI()->ClearBuffers(Pine::Graphics::Buffers::ColorBuffer);
        Pine::Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);

        Pine::Renderer3D::FrameReset();

        Pine::Renderer3D::SetCamera(cameraEntity->GetComponent<Pine::Camera>());
        Pine::Renderer3D::AddLight(lightEntity->GetComponent<Pine::Light>());
        Pine::Renderer3D::UploadLights();

        if (icon.Asset->GetType() == Pine::AssetType::Material)
        {
            RenderMaterial(icon);
        }
    }

    std::unordered_map<std::string, Icon> m_IconCache;
    Pine::Graphics::IFrameBuffer *m_PreviewFrameBuffer = nullptr;

    void OnRender(Pine::RenderingContext*, Pine::RenderStage stage, float)
    {
        if (stage != Pine::RenderStage::PreRender)
            return;

        for (auto &[path, icon]: m_IconCache)
        {
            if (icon.Asset->IsDeleted())
                continue;
            if (!icon.m_Dirty)
                continue;
            if (icon.Type == IconType::Static)
                continue;

            GenerateDynamicIcon(icon);

            icon.m_Dirty = false;
        }

    }
}

void IconStorage::Setup()
{
    m_PreviewFrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_PreviewFrameBuffer->Create(512, 512, Pine::Graphics::Buffers::ColorBuffer);

    Pine::RenderManager::AddRenderCallback(OnRender);
}

void IconStorage::Update()
{
    std::vector<std::string> removeList;

    // Find and remove unloaded assets from the icon cache
    for (auto &[iconAssetPath, icon]: m_IconCache)
    {
        auto asset = Pine::Assets::Get(iconAssetPath);

        if (asset && !asset->IsDeleted())
        {
            continue;
        }

        if (icon.Type == IconType::Dynamic)
        {
            Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(icon.FrameBuffer);
            icon.FrameBuffer = nullptr;
        }

        removeList.push_back(iconAssetPath);
    }

    for (const auto &icon: removeList)
    {
        m_IconCache.erase(icon);
    }

    // Generate icons
    for (const auto &[path, asset]: Pine::Assets::GetAll())
    {
        if (asset->IsDeleted())
            continue;

        Icon *icon = nullptr;

        icon = &m_IconCache[path];

        // If path is empty, it has just been created.
        if (icon->Path.empty())
        {
            icon->Path = path;
            icon->Asset = asset;
        }

        if (ShouldGenerateDynamicIcon(asset))
        {
            icon->Type = IconType::Dynamic;

            if (icon->FrameBuffer == nullptr)
            {
                icon->FrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
                icon->FrameBuffer->Create(128, 128, Pine::Graphics::Buffers::ColorBuffer);
            }

            icon->m_Dirty = true;
        }

        icon->Texture = GetStaticIconFromAsset(asset);
    }
}

Pine::Graphics::ITexture *IconStorage::GetIconTexture(const std::string &path)
{
    static auto invalidAssetIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file.png");

    if (!m_IconCache.count(path) || !m_IconCache[path].Texture)
    {
        return invalidAssetIcon->GetGraphicsTexture();
    }

    const auto& icon = m_IconCache[path];

    if (icon.Type == IconType::Dynamic)
    {
        return icon.FrameBuffer->GetColorBuffer();
    }

    return m_IconCache[path].Texture->GetGraphicsTexture();
}

void IconStorage::MarkIconDirty(const std::string &path)
{
    if (!m_IconCache.count(path))
    {
        return;
    }

    m_IconCache[path].m_Dirty = true;
}

void IconStorage::Dispose()
{
    for (auto &[iconAssetPath, icon]: m_IconCache)
    {
        if (icon.Type == IconType::Dynamic && icon.FrameBuffer != nullptr)
        {
            Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(icon.FrameBuffer);
            icon.FrameBuffer = nullptr;
        }
    }

    Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_PreviewFrameBuffer);

    m_PreviewFrameBuffer = nullptr;
}
