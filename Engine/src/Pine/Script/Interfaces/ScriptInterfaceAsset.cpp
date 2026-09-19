#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Script/Bindings/Bindings.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

namespace
{
    const char* GetFileName(Pine::UId id)
    {
        const auto asset = Pine::Assets::GetAssetByUId(id);
        if (!asset) return nullptr;
        return Pine::Script::Bindings::ReturnString(asset->GetFileName());
    }

    const char* GetPath(Pine::UId id)
    {
        const auto asset = Pine::Assets::GetAssetByUId(id);
        if (!asset) return nullptr;
        return Pine::Script::Bindings::ReturnString(asset->GetPath());
    }

    std::uint64_t GetByPath(const char* path)
    {
        const auto asset = Pine::Assets::Get<Pine::Asset>(path);

        if (!asset)
        {
            return 0;
        }

        return asset->GetScriptHandle()->Id;
    }

    std::uint64_t GetById(Pine::UId id)
    {
        const auto asset = Pine::Assets::GetAssetByUId(id);

        if (!asset)
        {
            return 0;
        }

        return asset->GetScriptHandle()->Id;
    }

    // -----------------------------------------------------

    float AudioGetDuration(Pine::UId id)
    {
        const auto audioFile = dynamic_cast<Pine::AudioFile*>(Pine::Assets::GetAssetByUId(id));
        if (!audioFile) return 0.f;
        return audioFile->GetDuration();
    }

    // -----------------------------------------------------

    // NOTE: The Blueprint/Level script interface below is still dormant — it predates the UId
    // migration and was never re-wired. Signatures now take the asset UId (so the managed side
    // binds correctly and resolution would go through Assets::GetAssetByUId), but the bodies
    // remain stubbed pending a separate pass to revive Blueprint/Level scripting.
    bool GetHasEntity(Pine::UId id)
    {
        return false;
        //return dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetAssetByUId(id))->HasEntity();
    }

    void CreateFromEntity(Pine::UId id, std::uint32_t entityId)
    {
        //auto asset = dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetAssetByUId(id));
        //asset->CreateFromEntity(Pine::Entities::GetByInternalId(entityId));
    }

    std::uint64_t SpawnEntity(Pine::UId id)
    {
        //return dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetAssetByUId(id))->Spawn()->GetScriptHandle()->Id;
        return 0;
    }

    void LevelCreateFromWorld(Pine::UId id)
    {
        //auto asset = dynamic_cast<Pine::Level*>(Pine::Assets::GetAssetByUId(id));

        //asset->CreateFromWorld();
    }

    void LevelLoad(Pine::UId id)
    {
        //auto asset = dynamic_cast<Pine::Level*>(Pine::Assets::GetAssetByUId(id));

        //asset->Load();
    }
}

void Pine::Script::Interfaces::Asset::Setup()
{
    Bindings::Register("Pine.Assets.Asset::GetFileName", GetFileName);
    Bindings::Register("Pine.Assets.Asset::GetPath", GetPath);
    Bindings::Register("Pine.Assets.AssetManager::GetByPath", GetByPath);
    Bindings::Register("Pine.Assets.AssetManager::GetById", GetById);
    Bindings::Register("Pine.Assets.Audio::GetDuration", AudioGetDuration);
    Bindings::Register("Pine.Assets.Blueprint::GetHasEntity", GetHasEntity);
    Bindings::Register("Pine.Assets.Blueprint::CreateFromEntity", CreateFromEntity);
    Bindings::Register("Pine.Assets.Blueprint::SpawnEntity", SpawnEntity);
    Bindings::Register("Pine.Assets.Level::CreateFromWorld", LevelCreateFromWorld);
    Bindings::Register("Pine.Assets.Level::Load", LevelLoad);
}