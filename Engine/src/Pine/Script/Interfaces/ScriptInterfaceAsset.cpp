#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
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
}

void Pine::Script::Interfaces::Asset::Setup()
{
    mono_add_internal_call("Pine.Assets.Asset::GetFileName", reinterpret_cast<void*>(GetFileName));
    mono_add_internal_call("Pine.Assets.Asset::GetPath", reinterpret_cast<void*>(GetPath));
    mono_add_internal_call("Pine.Assets.AssetManager::GetByPath", reinterpret_cast<void*>(GetByPath));
}