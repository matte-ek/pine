#include "IAsset.hpp"

const std::string& Pine::IAsset::GetFileName() const
{
    return m_FileName;
}

Pine::AssetType Pine::IAsset::GetType() const
{
    return m_Type;
}

void Pine::IAsset::SetPath(const std::string& path)
{
    m_Path = path;
}

const std::string& Pine::IAsset::GetPath() const
{
    return m_Path;
}

void Pine::IAsset::SetFilePath(std::filesystem::path path)
{
    m_FilePath = path;
}

const std::filesystem::path& Pine::IAsset::GetFilePath() const
{
    return m_FilePath;
}

Pine::AssetState Pine::IAsset::GetState() const
{
    return m_State;
}

bool Pine::IAsset::SaveToFile()
{
    return false;
}

bool Pine::IAsset::LoadFromFile()
{
    return false;
}

