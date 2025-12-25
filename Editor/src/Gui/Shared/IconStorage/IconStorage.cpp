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

        Pine::Texture2D *StaticTexture = nullptr;
        Pine::Graphics::IFrameBuffer *DynamicTexture = nullptr;

        IconType Type = IconType::Static;

        bool m_Dirty = true;
    };

    std::unordered_map<std::string, Icon> m_IconCache;

    Pine::Graphics::IFrameBuffer *m_PreviewFrameBuffer = nullptr;
    Pine::Graphics::IFrameBuffer *m_IconFrameBuffer = nullptr;

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

    bool ShouldGenerateDynamicIcon(const Pine::IAsset *asset)
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

    void RenderMaterial(const Icon &icon)
    {
        static auto sphereModel = Pine::Assets::Get<Pine::Model>("editor/models/sphere.fbx");
        static Pine::Entity* sphereEntity = nullptr;

        if (sphereEntity == nullptr)
        {
            sphereEntity = new Pine::Entity(0);
            sphereEntity->AddComponent(new Pine::Transform());

            sphereEntity->GetTransform()->LocalScale = Pine::Vector3f(10.f);
            sphereEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(0.f, -90.f, 0.f));
            sphereEntity->GetTransform()->LocalPosition = Pine::Vector3f(0, 0, -0.2f);
            sphereEntity->GetTransform()->OnRender(0.f);
        }

        Pine::Renderer3D::PrepareMesh(sphereModel->GetMeshes()[0], dynamic_cast<Pine::Material*>(icon.Asset));
        Pine::Renderer3D::RenderMesh(sphereEntity->GetTransform()->GetTransformationMatrix());
    }

    void RenderModel(const Icon& icon)
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

        modelEntity->GetTransform()->LocalScale = Pine::Vector3f(1.f);
        modelEntity->GetTransform()->LocalPosition = Pine::Vector3f(0, -center.y, -1.f);

        modelEntity->GetTransform()->OnRender(0.f);

        for (const auto& mesh : model->GetMeshes())
        {
            Pine::Renderer3D::PrepareMesh(mesh);
            Pine::Renderer3D::RenderMesh(modelEntity->GetTransform()->GetTransformationMatrix());
        }
    }

    void GenerateDynamicIcon(const Icon &icon)
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

        m_IconFrameBuffer->Bind();

        Pine::Graphics::GetGraphicsAPI()->SetViewport(Pine::Vector2i(0), Pine::Vector2i(64, 64));
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

        if (icon.Asset->GetType() == Pine::AssetType::Model)
        {
            RenderModel(icon);
        }

        icon.DynamicTexture->Blit(
            m_IconFrameBuffer,
            Pine::Graphics::ColorBuffer,
            Pine::Vector4i(0, 0, 64, 64),
            Pine::Vector4i(0, 64, 64, 0));
    }

    void OnRender(Pine::RenderingContext*, Pine::RenderStage stage, float)
    {
        if (stage != Pine::RenderStage::PreRender)
            return;

        for (auto &[path, icon] : m_IconCache)
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
    m_PreviewFrameBuffer->Prepare();
    m_PreviewFrameBuffer->AttachTextures(512, 512, Pine::Graphics::Buffers::ColorBuffer);
    m_PreviewFrameBuffer->Finish();

    m_IconFrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_IconFrameBuffer->Prepare();
    m_IconFrameBuffer->AttachTextures(64, 64, Pine::Graphics::Buffers::ColorBuffer);
    m_IconFrameBuffer->Finish();

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
            Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(icon.DynamicTexture);
            icon.DynamicTexture = nullptr;
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

Pine::Graphics::ITexture *IconStorage::GetIconTexture(const std::string &path)
{
    static auto invalidAssetIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file.png");

    if (!m_IconCache.count(path) || !m_IconCache[path].StaticTexture)
    {
        return invalidAssetIcon->GetGraphicsTexture();
    }

    const auto& icon = m_IconCache[path];

    if (icon.Type == IconType::Dynamic)
    {
        return icon.DynamicTexture->GetColorBuffer();
    }

    return m_IconCache[path].StaticTexture->GetGraphicsTexture();
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
