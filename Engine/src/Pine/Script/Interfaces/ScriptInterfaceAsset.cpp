#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include <mono/metadata/appdomain.h>

namespace
{
    MonoString* GetFileName(Pine::UId id)
    {
        const auto asset = Pine::Assets::GetAssetByUId(id);
        if (!asset) return nullptr;
        return mono_string_new(mono_domain_get(), asset->GetFileName().c_str());
    }

    MonoString* GetPath(Pine::UId id)
    {
        const auto asset = Pine::Assets::GetAssetByUId(id);
        if (!asset) return nullptr;
        return mono_string_new(mono_domain_get(), asset->GetPath().c_str());
    }

    MonoObject* GetByPath(MonoString* str)
    {
        auto asset = Pine::Assets::Get<Pine::Asset>(mono_string_to_utf8(str));

        if (!asset || asset->GetScriptHandle()->Object == nullptr)
        {
            return nullptr;
        }

        return mono_gchandle_get_target(asset->GetScriptHandle()->Handle);
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

    MonoObject* SpawnEntity(Pine::UId id)
    {
        //return mono_gchandle_get_target(dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetAssetByUId(id))->Spawn()->GetScriptHandle()->Handle);
        return nullptr;
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
    mono_add_internal_call("Pine.Assets.Asset::GetFileName", reinterpret_cast<void*>(GetFileName));
    mono_add_internal_call("Pine.Assets.Asset::GetPath", reinterpret_cast<void*>(GetPath));
    mono_add_internal_call("Pine.Assets.AssetManager::GetByPath", reinterpret_cast<void*>(GetByPath));
    mono_add_internal_call("Pine.Assets.Audio::GetDuration", reinterpret_cast<void*>(AudioGetDuration));
    mono_add_internal_call("Pine.Assets.Blueprint::GetHasEntity", reinterpret_cast<void*>(GetHasEntity));
    mono_add_internal_call("Pine.Assets.Blueprint::CreateFromEntity", reinterpret_cast<void*>(CreateFromEntity));
    mono_add_internal_call("Pine.Assets.Blueprint::SpawnEntity", reinterpret_cast<void*>(SpawnEntity));
    mono_add_internal_call("Pine.Assets.Level::CreateFromWorld", reinterpret_cast<void*>(LevelCreateFromWorld));
    mono_add_internal_call("Pine.Assets.Level::Load", reinterpret_cast<void*>(LevelLoad));
}