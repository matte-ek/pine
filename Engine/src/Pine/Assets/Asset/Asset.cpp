#include "Asset.hpp"

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/Threading/Threading.hpp"

bool Pine::Asset::LoadAssetData(const ByteSpan& span)
{
    return true;
}

Pine::ByteSpan Pine::Asset::SaveAssetData()
{
    return {nullptr, 0};
}

void Pine::Asset::SetupNew(const std::filesystem::path& absoluteFilePath)
{
    m_UId = UId::New();

    const auto universalPath = File::UniversalPath(absoluteFilePath);

    m_Path = String::StartsWith(universalPath, Assets::Internal::GetWorkingDirectory()) ?
        universalPath.substr(Assets::Internal::GetWorkingDirectory().length()) : universalPath;

    m_FilePath = std::filesystem::path(universalPath).replace_extension(".passet").string();
}

const Pine::UId& Pine::Asset::GetUId() const
{
    return m_UId;
}

const std::string& Pine::Asset::GetPath() const
{
    return m_Path;
}

const std::filesystem::path& Pine::Asset::GetFilePath() const
{
    return m_FilePath;
}

void Pine::Asset::AddSource(const std::string& filePath)
{
    AssetSource src;

    src.FilePath = filePath;
    src.LastWriteTime = std::filesystem::last_write_time(filePath).time_since_epoch().count();

    m_SourceFiles.push_back(src);
}

const Pine::AssetType& Pine::Asset::GetType() const
{
    return m_Type;
}

const Pine::AssetState& Pine::Asset::GetState() const
{
    return m_State;
}

void Pine::Asset::SetPath(const std::string& path)
{
    m_Path = File::UniversalPath(path);
}

void Pine::Asset::RemoveSource(const std::string& filePath)
{
}

const std::vector<Pine::AssetSource>& Pine::Asset::GetSources() const
{
    return m_SourceFiles;
}

void Pine::Asset::MarkAsModified()
{
    m_HasBeenModified = true;
}

bool Pine::Asset::HasBeenModified() const
{
    return m_HasBeenModified;
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

void Pine::Asset::SaveToFile()
{
    m_HasBeenModified = false;

    File::WriteCompressed(m_FilePath, Save());
}

void Pine::Asset::ReImport()
{
    // Run the underlying importer to load the new source data
    Import();

    // Get the new ready pine asset data
    auto newData = Save();

    // Write this new data to disk, if possible.
    if (!m_FilePath.empty())
    {
        File::WriteCompressed(m_FilePath, newData);
    }

    // Reload this data from memory, we do since assets will only save the newly imported data
    // within Save(), as such, these changes might not have been applied to loaded GPU textures and such.
    m_CreatedTime = 0;

    Load(newData);
}

Pine::Asset* Pine::Asset::Load(const ByteSpan& data, bool ignoreAssetData)
{
    return Load(data, "", ignoreAssetData);
}

Pine::Asset* Pine::Asset::Load(const ByteSpan& data, const std::string& filePath, bool ignoreAssetData)
{
    AssetSerializer aSerializer;

    if (!aSerializer.Read(data))
    {
        return nullptr;
    }

    Asset* asset = nullptr;

    // First try to just load the id to determine if this asset has been loaded already.
    if (const auto prevAsset = Assets::GetAssetByUId(aSerializer.UId.Read<UId>()))
    {
        // If so, check if this is the same version.
        if (prevAsset->m_CreatedTime == aSerializer.Time.Read<std::uint64_t>())
        {
            return prevAsset;
        }

        asset = prevAsset;
    }

    // Otherwise, create a new instance.
    if (!asset)
    {
        asset = Assets::Internal::CreateAssetByType(aSerializer.Type.Read<AssetType>());
        if (!asset)
        {
            return nullptr;
        }
    }

    aSerializer.UId.Read(asset->m_UId);
    aSerializer.Time.Read(asset->m_CreatedTime);
    aSerializer.Path.Read(asset->m_Path);

    asset->m_SourceFiles.clear();

    for (size_t i{};i < aSerializer.Sources.GetDataCount();i++)
    {
        AssetSource source;
        AssetSourceSerializer aSourceSerializer;

        aSourceSerializer.Read(aSerializer.Sources.GetData(i));

        aSourceSerializer.FilePath.Read(source.FilePath);
        aSourceSerializer.LastWriteTime.Read(source.LastWriteTime);

        asset->m_SourceFiles.push_back(source);
    }

    asset->m_FilePath = File::UniversalPath(filePath);

    if (ignoreAssetData)
    {
        return asset;
    }

    if (!asset->LoadAssetData(aSerializer.Data.Read()))
    {
        delete asset;
        return nullptr;
    }

    asset->m_State = AssetState::Loaded;

    return asset;
}

Pine::Asset* Pine::Asset::LoadFromFile(const std::filesystem::path& filePath, bool ignoreAssetData)
{
    if (!std::filesystem::exists(filePath))
    {
        return nullptr;
    }

    return Load(File::ReadCompressed(filePath), filePath.string(), ignoreAssetData);
}

bool Pine::Asset::Import()
{
    return true;
}

Pine::ByteSpan Pine::Asset::Save()
{
    AssetSerializer aSerializer;

    m_CreatedTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    aSerializer.UId.Write(m_UId);
    aSerializer.Time.Write(m_CreatedTime);
    aSerializer.Type.Write(m_Type);
    aSerializer.Path.Write(m_Path);

    for (size_t i{};i < m_SourceFiles.size();i++)
    {
        AssetSourceSerializer aSourceSerializer;

        aSourceSerializer.FilePath.Write(m_SourceFiles[i].FilePath);
        aSourceSerializer.LastWriteTime.Write(m_SourceFiles[i].LastWriteTime);

        aSerializer.Sources.AddData(aSourceSerializer.Write());
    }

    aSerializer.Data.Write(SaveAssetData());

    return aSerializer.Write();
}