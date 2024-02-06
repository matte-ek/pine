#include "ScriptingRuntime.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/Interfaces/Interfaces.hpp"

#include <mono/jit/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/tokentype.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/mono-config.h>
#include <vector>

namespace
{
    MonoDomain *m_Domain;
    MonoAssembly *m_PineAssembly;
    MonoImage *m_PineImage;

    std::vector<Pine::Script::RuntimeAssembly> m_Assemblies;
}

bool Pine::Script::Runtime::Setup()
{
    m_Domain = mono_jit_init("PineRuntime");

    if (!m_Domain)
    {
        Log::Error("Failed to initialize Mono runtime.");
        return false;
    }

    // TODO: Figure out how we'll handle this on Winblows
    mono_config_parse("/etc/mono/config");

    m_PineAssembly = mono_domain_assembly_open(m_Domain, "engine/script/Pine.dll");
    if (!m_PineAssembly)
    {
        Log::Error("Failed to open Pine assembly.");
        return false;
    }

    m_PineImage = mono_assembly_get_image(m_PineAssembly);

    Interfaces::Log::Setup();
    Interfaces::Entity::Setup();
    Interfaces::Transform::Setup();

    return true;
}

void Pine::Script::Runtime::Dispose()
{
    mono_jit_cleanup(m_Domain);
}

Pine::Script::RuntimeAssembly* Pine::Script::Runtime::LoadAssembly(const std::filesystem::path &path)
{
    if (!m_Domain)
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

    auto assembly = mono_domain_assembly_open(m_Domain, path.string().c_str());
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
    if (!m_Domain)
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
    return m_Domain;
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
