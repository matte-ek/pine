#include "AssetClipboard.hpp"

#include "Pine/Assets/Assets.hpp"

namespace
{
    std::vector<Pine::UId> m_CopiedAssetIds;
}

void Editor::Clipboard::Asset::Copy(const std::vector<Pine::Asset*>& assets)
{
    m_CopiedAssetIds.clear();

    for (const auto asset : assets)
    {
        m_CopiedAssetIds.push_back(asset->GetUId());
    }
}

bool Editor::Clipboard::Asset::HasData()
{
    return !m_CopiedAssetIds.empty();
}

std::vector<Pine::Asset*> Editor::Clipboard::Asset::GetAssets()
{
    std::vector<Pine::Asset*> assets;

    for (const auto& assetId : m_CopiedAssetIds)
    {
        if (const auto asset = Pine::Assets::GetAssetByUId(assetId))
        {
            assets.push_back(asset);
        }
    }

    return assets;
}
