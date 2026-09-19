#include "Asset.hpp"

#include <algorithm>

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Pine/Threading/Threading.hpp"

bool Pine::Asset::LoadAssetData(const ByteSpan& span)
{
    return true;
}

Pine::ByteSpan Pine::Asset::SaveAssetData()
{
    return {nullptr, 0};
}

Pine::ByteSpan Pine::Asset::ReadStoredAssetData() const
{
    if (m_FilePath.empty() || !std::filesystem::exists(m_FilePath))
    {
        return {};
    }

    AssetSerializer assetSerializer;

    if (!assetSerializer.Read(File::ReadCompressed(m_FilePath)))
    {
        PWarning(fmt::format("Could not read back the stored data of asset '{}'.", m_Path));

        return {};
    }

    return assetSerializer.Data.Read();
}

void Pine::Asset::IncreaseReference()
{
    ++m_ReferenceCount;
}

void Pine::Asset::DecreaseReference()
{
    if (m_IsPendingDelete && --m_ReferenceCount == 0)
    {
        delete this;
    }
}

void Pine::Asset::SetupNew(const std::filesystem::path& absoluteFilePath)
{
    m_UId = UId::New();

    const auto universalPath = File::UniversalPath(absoluteFilePath);

    m_Path = String::ToLower(String::StartsWith(universalPath, Assets::Internal::GetWorkingDirectory()) ?
        universalPath.substr(Assets::Internal::GetWorkingDirectory().length()) : universalPath);

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

std::string Pine::Asset::GetFileName() const
{
    return std::filesystem::path(GetFilePath()).replace_extension("").filename().string();
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
    const auto universalPath = File::UniversalPath(filePath);

    m_SourceFiles.erase(std::remove_if(m_SourceFiles.begin(), m_SourceFiles.end(),
        [&](const AssetSource& source)
        {
            return File::UniversalPath(source.FilePath) == universalPath;
        }), m_SourceFiles.end());
}

void Pine::Asset::ClearSources()
{
    m_SourceFiles.clear();
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

void Pine::Asset::MarkAsSaved()
{
    m_HasBeenModified = false;
}

void Pine::Asset::MarkPendingDelete()
{
    m_IsPendingDelete = true;

    if (m_ReferenceCount == 0)
    {
        delete this;
    }
}

bool Pine::Asset::IsPendingDelete() const
{
    return m_IsPendingDelete;
}

int Pine::Asset::GetReferenceCount() const
{
    return m_ReferenceCount;
}

void Pine::Asset::CreateScriptHandle()
{
    m_ScriptObjectHandle = Script::ObjectFactory::CreateAsset(this);
}

void Pine::Asset::DestroyScriptHandle()
{
    if (!m_ScriptObjectHandle.IsValid())
    {
        return;
    }

    Script::ObjectFactory::DisposeObject(&m_ScriptObjectHandle);
}

Pine::Script::ObjectHandle* Pine::Asset::GetScriptHandle()
{
    // Lazily create the managed mirror the first time script touches this asset - most assets
    // are never reached from C# at all, and the ones that are come and go with the project.
    if (!m_ScriptObjectHandle.IsValid() && Script::Runtime::IsAvailable())
    {
        CreateScriptHandle();
    }

    return &m_ScriptObjectHandle;
}

Pine::Asset::~Asset()
{
    DestroyScriptHandle();
}

void Pine::Asset::SaveToFile()
{
    MarkAsSaved();

    File::WriteCompressed(m_FilePath, Save());
}

void Pine::Asset::ReImport()
{
    // Run the underlying importer to load the new source data
    Import();
    ReLoad();
}

void Pine::Asset::ReLoad()
{
    // Get the new ready pine asset data
    const auto newData = Save();

    const AssetSerializer assetSerializer;

    assetSerializer.Read(newData);

    // Apply the new data before storing it. A source that no longer loads - a shader that stopped
    // compiling, say - must not replace the last good '.passet', or the next start would have
    // nothing usable to load and the asset would be broken until the source is fixed.
    if (!LoadAssetData(assetSerializer.Data.Read()))
    {
        PWarning(fmt::format("Failed to re-load asset '{}', keeping the previously stored data.", m_Path));
        return;
    }

    // Write this new data to disk, if possible.
    if (!m_FilePath.empty())
    {
        File::WriteCompressed(m_FilePath, newData);
    }
}

Pine::Asset* Pine::Asset::Load(const ByteSpan& data, const bool ignoreAssetData)
{
    return Load(data, "", ignoreAssetData);
}

Pine::Asset* Pine::Asset::Load(const ByteSpan& data, const std::string& filePath, const bool ignoreAssetData)
{
    AssetSerializer aSerializer;

    if (!aSerializer.Read(data))
    {
        return nullptr;
    }

    Asset* asset = nullptr;

    // First try to just load the id to determine if this asset has been loaded already.
    const auto prevAsset = Assets::GetAssetByUId(aSerializer.UId.Read<UId>());

    if (prevAsset)
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

    // This can probably get removed later.
    asset->m_Path = String::ToLower(asset->m_Path);

    asset->m_SourceFiles.clear();

    for (size_t i{};i < aSerializer.Sources.GetDataCount();i++)
    {
        AssetSource source;
        AssetSourceSerializer aSourceSerializer;

        aSourceSerializer.Read(aSerializer.Sources.GetData(i));

        aSourceSerializer.FilePath.Read(source.FilePath);
        aSourceSerializer.LastWriteTime.Read(source.LastWriteTime);

        source.FilePath = File::UniversalPath(source.FilePath);

        asset->m_SourceFiles.push_back(source);
    }

    asset->m_FilePath = File::UniversalPath(filePath);

    if (ignoreAssetData)
    {
        return asset;
    }

    if (!asset->LoadAssetData(aSerializer.Data.Read()))
    {
        // Only an instance created here is ours to destroy. One that was already registered is
        // still owned by the asset manager, and by everything holding a handle to it.
        if (asset != prevAsset)
        {
            delete asset;
        }

        return nullptr;
    }

    asset->m_State = AssetState::Loaded;

    return asset;
}

Pine::Asset* Pine::Asset::LoadFromFile(const std::filesystem::path& filePath, const bool ignoreAssetData)
{
    if (!std::filesystem::exists(filePath))
    {
        return nullptr;
    }

    return Load(File::ReadCompressed(filePath), filePath.string(), ignoreAssetData);
}

bool Pine::Asset::Import(Importer::AssetImport* context)
{
    return true;
}

void Pine::Asset::ResolveImportSettings(const Importer::AssetImport& import)
{
    // Most asset types have no import settings to work out.
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
