#include "Asset.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

bool Pine::Asset::LoadAssetData(const ByteSpan& span)
{
    return true;
}

void Pine::Asset::SetupNew(const std::string& path, const std::filesystem::path& filePath)
{
    m_Guid = Guid::New();
    m_Path = path;
    m_FilePath = filePath;
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

Pine::Asset* Pine::Asset::Load(const ByteSpan& data)
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

    return asset;
}

bool Pine::Asset::Import()
{
    return true;
}

Pine::ByteSpan Pine::Asset::Save()
{
    return {nullptr, 0};
}
