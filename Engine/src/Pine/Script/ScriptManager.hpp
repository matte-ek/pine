#pragma once

#include <filesystem>

namespace Pine::Script::Manager
{
    void Setup();
    void Dispose();

    void OnStart();
    void OnUpdate(float deltaTime);
    void OnRender(float deltaTime);

    void LoadGameAssembly(const std::filesystem::path& path);
    bool HasGameAssembly();

    void ReloadScripts();
    void ReloadGameAssembly();
}