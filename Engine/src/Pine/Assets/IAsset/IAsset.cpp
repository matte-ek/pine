#include "IAsset.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
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

bool Pine::IAsset::HasMetadata() const
{
    return m_HasMetadata;
}

bool Pine::IAsset::HasDependencies() const
{
    return m_HasDependencies;
}

const std::vector<std::string>& Pine::IAsset::GetDependencies() const
{
    return m_DependencyFiles;
}

void Pine::IAsset::LoadMetadata()
{
    if (!HasFile())
        return;

    const auto metadataFile = m_FilePath.string() + ".asset";

    if (!std::filesystem::exists(metadataFile))
        return;

    auto fileContentsJson = Serialization::LoadFromFile(metadataFile);

    if (!fileContentsJson.has_value())
        return;

    // Load dependencies
    if (fileContentsJson.value().contains("dependencies"))
    {
        m_HasDependencies = true;
        m_DependencyFiles = fileContentsJson.value()["dependencies"];
    }

    // Load custom metadata set by the asset
    if (fileContentsJson.value().contains("data"))
        m_Metadata = fileContentsJson.value()["data"];
}

void Pine::IAsset::SaveMetadata()
{
    if (!(m_HasMetadata || m_HasDependencies))
        return;
    if (!HasFile())
        return;

    const auto metadataFile = m_FilePath.string() + ".asset";

    nlohmann::json outputJson;

    if (m_HasDependencies)
        outputJson["dependencies"] = m_DependencyFiles;
    if (m_HasMetadata)
        outputJson["data"] = m_Metadata;

    Serialization::SaveToFile(metadataFile, outputJson);
}
