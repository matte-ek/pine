#include "IAsset.hpp"
#include "Pine/Core/String/String.hpp"

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
    // For cross-platform compatibility, always use forward slash
    m_Path = String::Replace(path, "\\", "/");
    m_FileName = std::filesystem::path(path).filename().string();
}

const std::string& Pine::IAsset::GetPath() const
{
    return m_Path;
}

void Pine::IAsset::SetFilePath(const std::filesystem::path& path)
{
    m_FilePath = path;
    m_HasFile = true;
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

Pine::AssetLoadMode Pine::IAsset::GetLoadMode() const
{
    return m_LoadMode;
}

bool Pine::IAsset::LoadFromFile(AssetLoadStage stage) // NOLINT(google-default-arguments)
{
    return false;
}

bool Pine::IAsset::HasFile() const
{
    return m_HasFile;
}
