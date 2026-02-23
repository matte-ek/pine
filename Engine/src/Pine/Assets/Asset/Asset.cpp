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

void Pine::Asset::SetupNew(const std::string& path)
{
    m_UId = UId::New();
    m_Path = File::UniversalPath(path);
    m_FilePath = File::UniversalPath(fmt::format("{}data/{}.passet", Assets::Internal::GetWorkingDirectory(), m_UId.ToString()));
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
    File::WriteCompressed(m_FilePath, Save());
}

void Pine::Asset::ReImport()
{
    Import();
    LoadAssetData(SaveAssetData());
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

    auto asset = Assets::Internal::CreateAssetByType(aSerializer.Type.Read<AssetType>());
    if (!asset)
    {
        return nullptr;
    }

    aSerializer.UId.Read(asset->m_UId);
    aSerializer.Path.Read(asset->m_Path);

    aSerializer.Dependencies.Read(asset->m_Dependencies);

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

    if (!asset->m_Dependencies.empty())
    {
        for (const auto& dep : asset->m_Dependencies)
        {
            // We don't really care about the result right now, we can get that later.
            // Important thing is that we've loaded the asset.
            Assets::LoadAssetFromFile(fmt::format("data/{}.passet", dep.ToString()));
        }
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

    aSerializer.UId.Write(m_UId);
    aSerializer.Type.Write(m_Type);
    aSerializer.Path.Write(m_Path);
    aSerializer.Dependencies.Write(m_Dependencies);

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