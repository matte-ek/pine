#include "ScriptingRuntime.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/Interfaces/Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Script/ScriptManager.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"

#include <mono/jit/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/mono-config.h>
#include <vector>

namespace
{
    MonoDomain *m_RootDomain;
    MonoDomain *m_AppDomain;
    MonoAssembly *m_PineAssembly;
    MonoImage *m_PineImage;

    std::vector<Pine::Script::RuntimeAssembly> m_Assemblies;

    bool m_IsReloading = false;
    int m_ReloadIndex = 0;
}

bool Pine::Script::Runtime::Setup()
{
    if (!m_IsReloading)
    {
        m_RootDomain = mono_jit_init("PineRuntime");

        if (!m_RootDomain)
        {
            Log::Error("Failed to initialize Mono runtime.");
            return false;
        }
    }

    char buff[64];

    sprintf(buff, "PineAppDomain%d", m_ReloadIndex++);

    m_AppDomain = mono_domain_create_appdomain(const_cast<char*>(buff), nullptr);

    mono_domain_set(m_AppDomain, false);

    // TODO: Figure out how we'll handle this on Winblows
    mono_config_parse("/etc/mono/config");

    m_PineAssembly = mono_domain_assembly_open(m_AppDomain, "engine/script/Pine.dll");
    if (!m_PineAssembly)
    {
        Log::Error("Failed to open Pine assembly.");
        return false;
    }

    m_PineImage = mono_assembly_get_image(m_PineAssembly);

    Interfaces::Log::Setup();
    Interfaces::Entity::Setup();
    Interfaces::Component::Setup();
    Interfaces::Asset::Setup();
    Interfaces::Input::Setup();
    ObjectFactory::Setup();

    if (m_IsReloading)
    {
        for (const auto& [assetPath, asset] : Assets::GetAll())
        {
            if (asset->IsDeleted())
                continue;

            asset->CreateScriptHandle();
        }

        for (const auto& entity : Entities::GetList())
        {
            entity->CreateScriptHandle();

            for (const auto& component : entity->GetComponents())
            {
                component->CreateScriptInstance();
            }
        }
    }

    return true;
}

void Pine::Script::Runtime::Dispose()
{
    for (const auto& [assetPath, asset] : Assets::GetAll())
    {
        if (asset->GetType() == AssetType::CSharpScript)
        {
            auto script = dynamic_cast<CSharpScript*>(asset);

            if (script->GetScriptHandle()->Object != nullptr)
                mono_gchandle_free(script->GetScriptHandle()->Handle);
        }

        asset->DestroyScriptHandle();
    }

    for (const auto& entity : Entities::GetList())
    {
        entity->DestroyScriptHandle();

        for (const auto& component : entity->GetComponents())
        {
            if (component->GetType() == ComponentType::Script)
            {
                auto scriptComponent = dynamic_cast<ScriptComponent*>(component);

                scriptComponent->DestroyInstance();
            }

            component->DestroyScriptInstance();
        }
    }

    mono_domain_set(mono_get_root_domain(), false);
    mono_domain_finalize(m_AppDomain, 0);
    mono_domain_unload(m_AppDomain);
    //mono_jit_cleanup(m_AppDomain);

//    RunGarbageCollector();

    m_AppDomain = nullptr;
    m_IsReloading = true;
    m_Assemblies.clear();
}

Pine::Script::RuntimeAssembly* Pine::Script::Runtime::LoadAssembly(const std::filesystem::path &path)
{
    if (!m_RootDomain)
    {
        Log::Error("Mono runtime not initialized.");
        return nullptr;
    }

    for (const auto &assembly: m_Assemblies)
    {
        if (assembly.Path == path)
        {
            Log::Error(fmt::format("Assembly already loaded: {}", path.string()));
            return nullptr;
        }
    }

    auto assembly = mono_domain_assembly_open(m_AppDomain, path.string().c_str());
    if (!assembly)
    {
        Log::Error(fmt::format("Failed to open assembly: {}", path.string()));
        return nullptr;
    }

    auto image = mono_assembly_get_image(assembly);
    if (!image)
    {
        Log::Error(fmt::format("Failed to get image from assembly: {}", path.string()));
        return nullptr;
    }

    RuntimeAssembly runtimeAssembly = {path, assembly, image};

    m_Assemblies.push_back(runtimeAssembly);

    return &m_Assemblies.back();
}

bool Pine::Script::Runtime::UnloadAssembly(Pine::Script::RuntimeAssembly *assembly)
{
    if (!m_RootDomain)
    {
        Log::Error("Mono runtime not initialized.");
        return false;
    }

    if (!assembly)
    {
        Log::Error("Assembly is null.");
        return false;
    }

    if (assembly->Assembly)
    {
        mono_assembly_close(assembly->Assembly);
    }
    else
    {
        return false;
    }

    for (int i = 0; i < m_Assemblies.size(); i++)
    {
        if (&m_Assemblies[i] == assembly)
        {
            m_Assemblies.erase(m_Assemblies.begin() + i);
            return true;
        }
    }

    return false;
}

MonoAssembly *Pine::Script::Runtime::GetPineAssembly()
{
    return m_PineAssembly;
}

MonoDomain *Pine::Script::Runtime::GetDomain()
{
    return m_AppDomain;
}

MonoImage *Pine::Script::Runtime::GetPineImage()
{
    return m_PineImage;
}

void Pine::Script::Runtime::RunGarbageCollector()
{
    Log::Verbose("Running mono garbage collector...");

    mono_gc_collect(mono_gc_max_generation());

    Log::Verbose(fmt::format("mono_gc_get_heap_size(): {}", mono_gc_get_heap_size()));
}

void Pine::Script::Runtime::Reset()
{
    Dispose();
    Setup();
}
