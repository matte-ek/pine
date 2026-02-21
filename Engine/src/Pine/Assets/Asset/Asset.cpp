#include "Asset.hpp"

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

bool Pine::Asset::LoadAssetData(const ByteSpan& span)
{
    return true;
}

Pine::ByteSpan Pine::Asset::SaveAssetData()
{
    return {nullptr, 0};
}

void Pine::Asset::SetupNew(const std::string& path, const std::filesystem::path& filePath)
{
    m_Guid = Guid::New();
    m_Path = File::UniversalPath(path);
    m_FilePath = File::UniversalPath(filePath.string());
}

const Pine::Guid& Pine::Asset::GetGuid() const
{
    return m_Guid;
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

void Pine::Asset::RemoveSource(const std::string& filePath)
{

}

const std::vector<Pine::AssetSource>& Pine::Asset::GetSources() const
{
    return m_SourceFiles;
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

Pine::Asset* Pine::Asset::Load(const ByteSpan& data)
{
    return Load(data, "");
}

Pine::Asset* Pine::Asset::Load(const ByteSpan& data, const std::string& filePath)
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

    aSerializer.Guid.Read(asset->m_Guid);
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

    if (!asset->LoadAssetData(aSerializer.Data.Read()))
    {
        delete asset;
        return nullptr;
    }

    asset->m_FilePath = File::UniversalPath(filePath);

    return asset;
}

bool Pine::Asset::Import()
{
    return true;
}

Pine::ByteSpan Pine::Asset::Save()
{
    AssetSerializer aSerializer;

    aSerializer.Guid.Write(m_Guid);
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