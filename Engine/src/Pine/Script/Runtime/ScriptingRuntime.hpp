#pragma once
#include <filesystem>
#include <mono/metadata/image.h>
#include <mono/utils/mono-forward.h>

namespace Pine::Script
{
    struct RuntimeAssembly
    {
        std::filesystem::path Path;

        MonoAssembly *Assembly;
        MonoImage *Image;
    };
}

namespace Pine::Script::Runtime
{
    RuntimeAssembly* LoadAssembly(const std::filesystem::path& path);
    bool UnloadAssembly(const RuntimeAssembly* assembly);

    MonoDomain* GetDomain();
    MonoAssembly* GetPineAssembly();
    MonoImage* GetPineImage();

    void RunGarbageCollector();

    bool Setup();
    void Reset();
    void Dispose();

    bool IsAvailable();
}