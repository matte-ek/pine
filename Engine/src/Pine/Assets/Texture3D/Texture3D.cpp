#include "Texture3D.hpp"

#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::Texture3D::Texture3D()
{
    m_Type = AssetType::Texture3D;
    m_LoadMode = AssetLoadMode::SingleThread;
}

bool Pine::Texture3D::LoadFromFile(AssetLoadStage stage)
{
    auto j = Serialization::LoadFromFile(m_FilePath);

    if (!j.has_value())
    {
        return false;
    }

    const auto json = j.value();

    Serialization::LoadAsset(json, "texture", m_Texture, false);

    for (int i = 0; i < 6;i++)
    {
        Serialization::LoadAsset(json["side"], std::to_string(i), m_SideTextures[i], false);
    }

    Build();

    return true;
}

bool Pine::Texture3D::SaveToFile()
{
    nlohmann::json j;

    j["texture"] = Serialization::StoreAsset(m_Texture.Get());

    if (m_Texture.Get())
    {
        m_DependencyFiles.push_back(m_Texture->GetFilePath().string());
    }

    for (int i = 0; i < 6;i++)
    {
        j["side"][std::to_string(i)] = Serialization::StoreAsset(m_SideTextures[i].Get());

        if (m_SideTextures[i].Get())
        {
            m_DependencyFiles.push_back(m_SideTextures[i]->GetFilePath().string());
        }
    }

    m_HasDependencies = true;

    Serialization::SaveToFile(m_FilePath, j);

    return true;
}

void Pine::Texture3D::Dispose()
{
    if (m_CubeMapTexture)
    {
        Graphics::GetGraphicsAPI()->DestroyTexture(m_CubeMapTexture);

        m_CubeMapTexture = nullptr;
    }
}

void Pine::Texture3D::SetTexture(Texture2D*texture)
{
    m_Texture = texture;
}

Pine::Texture2D *Pine::Texture3D::GetTexture() const
{
    return m_Texture.Get();
}

void Pine::Texture3D::SetSideTexture(TextureCubeSide side, Texture2D*texture)
{
    m_SideTextures[static_cast<int>(side)] = texture;
}

Pine::Texture2D *Pine::Texture3D::GetSideTexture(TextureCubeSide side) const
{
    return m_SideTextures[static_cast<int>(side)].Get();
}

Pine::Graphics::ITexture *Pine::Texture3D::GetCubeMap() const
{
    return m_CubeMapTexture;
}

void Pine::Texture3D::Build()
{
    // Make sure we have all textures required
    if (m_Texture.Get() == nullptr)
    {
        for (int i = 0; i < 6;i++)
        {
            if (m_SideTextures[i].Get() == nullptr)
            {
                Log::Warning("Cannot build cube map, missing textures.");

                return;
            }
        }
    }

    // Dispose the previous cube map texture if any
    if (m_CubeMapTexture)
    {
        Graphics::GetGraphicsAPI()->DestroyTexture(m_CubeMapTexture);
    }

    // Create and prepare a new cube map
    m_CubeMapTexture = Graphics::GetGraphicsAPI()->CreateTexture();
    m_CubeMapTexture->SetType(Graphics::TextureType::CubeMap);
    m_CubeMapTexture->Bind();

    if (m_Texture.Get() != nullptr)
    {
        // TODO: Build cube map with the single texture
    }
    else
    {
        // Build cube map via 6 individual textures
        for (int i = 0; i < 6;i++)
        {
            auto side = m_SideTextures[i].Get();

            if (side->GetState() != AssetState::Loaded)
            {
                Log::Error("Attempted to build cube map with unloaded textures.");

                return;
            }

            m_CubeMapTexture->CopyTextureData(side->GetGraphicsTexture(), static_cast<Graphics::TextureUploadTarget>(i + 1));
        }
    }

    m_CubeMapTexture->SetFilteringMode(Graphics::TextureFilteringMode::Linear);
}


