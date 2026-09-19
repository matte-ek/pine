#include "ScriptingRuntime.hpp"

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/Bindings/Bindings.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/Script/GameAssembly/GameAssembly.hpp"
#include "../ManagedCall/ManagedCall.hpp"
#include "Pine/Script/Scripts/ScriptFieldRegistry.hpp"

#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>

#include <dlfcn.h>
#include <filesystem>
#include <vector>

namespace
{
    // The engine's own managed assembly, and the configuration CoreCLR is started from, relative
    // to the working directory - which is the data/ tree every application runs from.
    constexpr auto PineAssemblyPath = "engine/script/Pine.dll";
    constexpr auto PineRuntimeConfigPath = "engine/script/Pine.runtimeconfig.json";

    constexpr auto InteropTypeName = "Pine.Core.Interop, Pine";

    void* m_HostFxrLibrary = nullptr;
    hostfxr_handle m_HostContext = nullptr;
    hostfxr_close_fn m_CloseHost = nullptr;

    void (*m_RunGarbageCollector)() = nullptr;

    bool m_IsAvailable = false;

    // libhostfxr is not linked against, because where it lives depends on which .NET is
    // installed. nethost answers that, and the three entry points the engine needs come out of
    // the library by name.
    bool LoadHostFxr()
    {
        char path[2048];
        auto length = sizeof(path) / sizeof(char);

        if (get_hostfxr_path(path, &length, nullptr) != 0)
        {
            PWarning("Script: No .NET runtime host found on this machine, scripting is off. "
                     "Install a .NET runtime to turn it on.");

            return false;
        }

        m_HostFxrLibrary = dlopen(path, RTLD_LAZY | RTLD_LOCAL);

        if (m_HostFxrLibrary == nullptr)
        {
            PError(fmt::format("Script: Failed to load the .NET runtime host at {}: {}",
                path, dlerror()));

            return false;
        }

        return true;
    }

    template <typename Signature>
    Signature HostFxrFunction(const char* name)
    {
        return reinterpret_cast<Signature>(dlsym(m_HostFxrLibrary, name));
    }

    // Both hosting calls want a path rather than something to resolve, and hdt_load_assembly
    // documents its own as fully qualified. The two are resolved together so there is no
    // remembering which of them is the fussy one.
    std::string AbsolutePath(const char* path)
    {
        return std::filesystem::absolute(path).string();
    }

    // Start CoreCLR from Pine.runtimeconfig.json and hand back the two delegates the engine
    // works through: one to load an assembly into the default context, one to resolve a method.
    bool StartRuntime(load_assembly_fn& loadAssembly, get_function_pointer_fn& getFunctionPointer)
    {
        const auto initialize = HostFxrFunction<hostfxr_initialize_for_runtime_config_fn>(
            "hostfxr_initialize_for_runtime_config");
        const auto getDelegate = HostFxrFunction<hostfxr_get_runtime_delegate_fn>(
            "hostfxr_get_runtime_delegate");

        m_CloseHost = HostFxrFunction<hostfxr_close_fn>("hostfxr_close");

        if (initialize == nullptr || getDelegate == nullptr || m_CloseHost == nullptr)
        {
            PError("Script: The .NET runtime host is missing the entry points the engine starts "
                   "it through, scripting system inoperational.");

            return false;
        }

        const auto configPath = AbsolutePath(PineRuntimeConfigPath);

        if (initialize(configPath.c_str(), nullptr, &m_HostContext) != 0 || m_HostContext == nullptr)
        {
            PError(fmt::format("Script: Failed to start the .NET runtime from {}, scripting "
                               "system inoperational.", configPath));

            return false;
        }

        // hdt_load_assembly, and deliberately not hdt_load_assembly_and_get_function_pointer:
        // that one puts each assembly in a private load context of its own, and a game assembly
        // resolving Pine from the default context would then find nothing - or a second copy,
        // whose Script class is a different type from the one the object factory knows.
        if (getDelegate(m_HostContext, hdt_load_assembly, reinterpret_cast<void**>(&loadAssembly)) != 0
            || getDelegate(m_HostContext, hdt_get_function_pointer, reinterpret_cast<void**>(&getFunctionPointer)) != 0)
        {
            PError("Script: The .NET runtime host would not hand over the delegates the engine "
                   "loads managed code through, scripting system inoperational.");

            return false;
        }

        return true;
    }

    // Tell Pine.dll how to reach the engine. Everything C# calls goes through the one function
    // this hands over - see Script/Bindings/Bindings.hpp - so it has to happen before any other
    // managed code runs, including anything that would like to log why it failed.
    bool BindPineAssembly()
    {
        using InitializeFn = std::int32_t (*)(void* (*)(const char*));

        const auto initialize = Pine::Script::ManagedCall::Find<InitializeFn>(
            InteropTypeName, "Initialize");

        if (initialize == nullptr)
        {
            return false;
        }

        Pine::Script::Bindings::Setup();

        if (initialize(Pine::Script::Bindings::Resolve) != 1)
        {
            PError("Script: Pine.dll could not bind itself to the engine, scripting system "
                   "inoperational.");

            return false;
        }

        return true;
    }
}

bool Pine::Script::Runtime::Setup()
{
    if (!LoadHostFxr())
    {
        return false;
    }

    load_assembly_fn loadAssembly = nullptr;
    get_function_pointer_fn getFunctionPointer = nullptr;

    if (!StartRuntime(loadAssembly, getFunctionPointer))
    {
        return false;
    }

    const auto assemblyPath = AbsolutePath(PineAssemblyPath);

    if (loadAssembly(assemblyPath.c_str(), nullptr, nullptr) != 0)
    {
        PError(fmt::format("Script: Failed to load the Pine engine assembly at {}, scripting "
                           "system inoperational.", assemblyPath));

        return false;
    }

    ManagedCall::Setup(getFunctionPointer);

    if (!BindPineAssembly())
    {
        return false;
    }

    m_RunGarbageCollector = ManagedCall::Find<void (*)()>(InteropTypeName, "RunGarbageCollector");

    ObjectFactory::Setup();
    GameAssembly::Setup();

    // The one seam that can refuse: it checks that the engine and Pine.dll still agree on the
    // shape of a field descriptor. Scripts run either way; their fields would not be reflected.
    if (!FieldRegistry::Setup())
    {
        PError("Script: Script fields will not be reflected.");
    }

    m_IsAvailable = true;

    return true;
}

void Pine::Script::Runtime::Dispose()
{
    // Nothing managed may be created or freed from here on, which is why Pine::Engine::Shutdown
    // leaves this until after the entities, components and assets have let go of their mirrors.
    m_IsAvailable = false;

    if (m_HostContext != nullptr)
    {
        m_CloseHost(m_HostContext);

        m_HostContext = nullptr;
    }

    // libhostfxr is deliberately left loaded. Unloading it while the runtime it started is still
    // in the process is a good way to crash on the way out, and the process is ending anyway.
}

void Pine::Script::Runtime::RunGarbageCollector()
{
    if (m_RunGarbageCollector == nullptr)
    {
        return;
    }

    m_RunGarbageCollector();
}

bool Pine::Script::Runtime::IsAvailable()
{
    return m_IsAvailable;
}
