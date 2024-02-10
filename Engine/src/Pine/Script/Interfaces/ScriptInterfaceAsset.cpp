#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include <mono/metadata/appdomain.h>

namespace
{
    MonoString* GetFileName(std::uint32_t internalId)
    {
        return mono_string_new(mono_domain_get(), Pine::Assets::GetById(internalId)->GetFileName().c_str());
    }

    MonoString* GetPath(std::uint32_t internalId)
    {
        return mono_string_new(mono_domain_get(), Pine::Assets::GetById(internalId)->GetPath().c_str());
    }

    MonoObject* GetByPath(MonoString* str)
    {
        auto asset = Pine::Assets::Get(mono_string_to_utf8(str));

        if (!asset || asset->GetScriptHandle()->Object == nullptr)
        {
            return nullptr;
        }

        return mono_gchandle_get_target(asset->GetScriptHandle()->Handle);
    }

    // -----------------------------------------------------

    bool GetHasEntity(std::uint32_t internalId)
    {
        return dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetById(internalId))->HasEntity();
    }

    void CreateFromEntity(std::uint32_t internalId, std::uint32_t entityId)
    {
        auto asset = dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetById(internalId));

        asset->CreateFromEntity(Pine::Entities::GetByInternalId(entityId));
    }

    MonoObject* SpawnEntity(std::uint32_t internalId)
    {
        return mono_gchandle_get_target(dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetById(internalId))->Spawn()->GetScriptHandle()->Handle);
    }

    void LevelCreateFromWorld(std::uint32_t internalId)
    {
        auto asset = dynamic_cast<Pine::Level*>(Pine::Assets::GetById(internalId));

        asset->CreateFromWorld();
    }

    void LevelLoad(std::uint32_t internalId)
    {
        auto asset = dynamic_cast<Pine::Level*>(Pine::Assets::GetById(internalId));

        asset->Load();
    }
}

void Pine::Script::Interfaces::Asset::Setup()
{
    mono_add_internal_call("Pine.Assets.Asset::GetFileName", reinterpret_cast<void*>(GetFileName));
    mono_add_internal_call("Pine.Assets.Asset::GetPath", reinterpret_cast<void*>(GetPath));
    mono_add_internal_call("Pine.Assets.AssetManager::GetByPath", reinterpret_cast<void*>(GetByPath));
    mono_add_internal_call("Pine.Assets.Blueprint::GetHasEntity", reinterpret_cast<void*>(GetHasEntity));
    mono_add_internal_call("Pine.Assets.Blueprint::CreateFromEntity", reinterpret_cast<void*>(CreateFromEntity));
    mono_add_internal_call("Pine.Assets.Blueprint::SpawnEntity", reinterpret_cast<void*>(SpawnEntity));
    mono_add_internal_call("Pine.Assets.Level::CreateFromWorld", reinterpret_cast<void*>(LevelCreateFromWorld));
    mono_add_internal_call("Pine.Assets.Level::Load", reinterpret_cast<void*>(LevelLoad));
}