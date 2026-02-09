#include "Asset.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

std::uint32_t Pine::Asset::GetId() const
{
    return m_Id;
}

void Pine::Asset::SetId(std::uint32_t id)
{
    m_Id = id;
}

void Pine::Asset::CreateScriptHandle()
{
    m_ScriptObjectHandle = Script::ObjectFactory::CreateAsset(this);
}

void Pine::Asset::DestroyScriptHandle()
{
    if (m_ScriptObjectHandle.Object == nullptr)
    {
        return;
    }

    Script::ObjectFactory::DisposeObject(&m_ScriptObjectHandle);
}

Pine::Script::ObjectHandle* Pine::Asset::GetScriptHandle()
{
    return &m_ScriptObjectHandle;
}

const std::string& Pine::Asset::GetFileName() const
{
    return m_FileName;
}

Pine::AssetType Pine::Asset::GetType() const
{
    return m_Type;
}

void Pine::Asset::SetPath(const std::string& path)
{
    // For cross-platform compatibility, always use forward slash
    m_Path = String::Replace(path, "\\", "/");
    m_FileName = std::filesystem::path(path).filename().string();
}

const std::string& Pine::Asset::GetPath() const
{
    return m_Path;
}

void Pine::Asset::SetFilePath(const std::filesystem::path& path)
{
    SetFilePath(path, "");
}

void Pine::Asset::SetFilePath(const std::filesystem::path& path, const std::filesystem::path& root)
{
    m_FilePath = String::Replace(path.string(), "\\", "/");
    m_FileRootPath = String::Replace(root.string(), "\\", "/");
    m_HasFile = true;
}

const std::filesystem::path& Pine::Asset::GetFilePath() const
{
    return m_FilePath;
}

const std::filesystem::path& Pine::Asset::GetFileRootPath() const
{
    return m_FileRootPath;
}

Pine::AssetState Pine::Asset::GetState() const
{
    return m_State;
}

bool Pine::Asset::SaveToFile()
{
    return false;
}

Pine::AssetLoadMode Pine::Asset::GetLoadMode() const
{
    return m_LoadMode;
}

bool Pine::Asset::LoadFromFile(AssetLoadStage stage) // NOLINT(google-default-arguments)
{
    return true;
}

bool Pine::Asset::HasFile() const
{
    return m_HasFile;
}

bool Pine::Asset::HasMetadata() const
{
    return m_HasMetadata;
}

bool Pine::Asset::HasDependencies() const
{
    return m_HasDependencies;
}

const std::vector<std::string>& Pine::Asset::GetDependencies() const
{
    return m_DependencyFiles;
}

void Pine::Asset::LoadMetadata()
{
    if (!HasFile())
        return;

    const auto metadataFile = m_FilePath.string() + ".asset";

    if (!std::filesystem::exists(metadataFile))
        return;

    auto fileContentsJson = SerializationJson::LoadFromFile(metadataFile);

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
    {
        m_Metadata = fileContentsJson.value()["data"];
    }
}

void Pine::Asset::SaveMetadata()
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

    SerializationJson::SaveToFile(metadataFile, outputJson);
}

void Pine::Asset::MarkAsUpdated()
{
    if (!m_HasFile)
    {
        throw std::runtime_error("Attempted to update read time on non-existing file.");
    }

    m_DiskWriteTime = last_write_time(m_FilePath);
    m_HasBeenModified = false;
}

bool Pine::Asset::HasBeenUpdated() const
{
    return last_write_time(m_FilePath) != m_DiskWriteTime;
}

bool Pine::Asset::IsDeleted() const
{
    return m_IsDeleted;
}

void Pine::Asset::MarkAsDeleted()
{
    m_IsDeleted = true;
}

bool Pine::Asset::IsModified() const
{
    return m_HasBeenModified;
}

void Pine::Asset::MarkAsModified()
{
    m_HasBeenModified = true;
}
