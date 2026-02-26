#include "Texture3D.hpp"

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Threading/Threading.hpp"

bool Pine::Texture3D::LoadAssetData(const ByteSpan& span)
{
    Texture3DSerializer textureSerializer;

    if (!textureSerializer.Read(span))
    {
        return false;
    }

    bool hasReadyCubeMap = true;

    for (size_t i = 0; i < 6; i++)
    {
        m_SideTextures[i] = textureSerializer.SideTextures.ReadElement<UId>(i);

        if (m_SideTextures[i] == UId::Empty())
        {
            hasReadyCubeMap = false;
        }
    }

    if (hasReadyCubeMap)
    {
        auto task = Threading::QueueTask<void>([this]()
        {
            Build();
        }, TaskThreadingMode::MainThread);

        Threading::AwaitTaskResult(task);
    }

    return true;
}

Pine::ByteSpan Pine::Texture3D::SaveAssetData()
{
    Texture3DSerializer textureSerializer;

    textureSerializer.SideTextures.SetSize(m_SideTextures.size());

    for (size_t i{}; i < m_SideTextures.size(); i++)
    {
        bool sideTextureValid = m_SideTextures[i].Get() != nullptr;
        textureSerializer.SideTextures.WriteElement(i, sideTextureValid ? m_SideTextures[i]->GetUId() : UId::Empty());
    }

    return textureSerializer.Write();
}

Pine::Texture3D::Texture3D()
{
    m_Type = AssetType::Texture3D;
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

bool Pine::Texture3D::IsReady() const
{
    return m_Ready;
}

bool Pine::Texture3D::Build()
{
    // Make sure we have all textures required
    for (int i = 0; i < 6;i++)
    {
        if (m_SideTextures[i].Get() == nullptr)
        {
            m_Ready = false;
            return false;
        }
    }

    PINE_PF_SCOPE();

    // Dispose the previous cube map texture if any
    if (m_CubeMapTexture)
    {
        Graphics::GetGraphicsAPI()->DestroyTexture(m_CubeMapTexture);
    }

    // Create and prepare a new cube map
    m_CubeMapTexture = Graphics::GetGraphicsAPI()->CreateTexture();
    m_CubeMapTexture->SetType(Graphics::TextureType::CubeMap);
    m_CubeMapTexture->Bind();

    // Build cube map via 6 individual textures
    for (int i = 0; i < 6;i++)
    {
        auto side = m_SideTextures[i].Get();

        if (side->GetState() != AssetState::Loaded)
        {
            Log::Error("Attempted to build cube map with unloaded textures.");
            m_Ready = false;
            return false;
        }

        m_CubeMapTexture->CopyTextureData(side->GetGraphicsTexture(), static_cast<Graphics::TextureUploadTarget>(i + 1));
    }

    m_CubeMapTexture->SetFilteringMode(Graphics::TextureFilteringMode::Linear);

    m_Ready = true;

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
