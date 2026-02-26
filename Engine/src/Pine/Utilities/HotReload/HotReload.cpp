#include "HotReload.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Engine/Engine.hpp"

#include <GLFW/glfw3.h>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Asset/Asset.hpp"

namespace
{
    std::vector<Pine::AssetHandle<Pine::Asset>> m_EngineAssets;

    void UpdateAssets()
    {
        int reloadedAssets = 0;

        // Find any source changes for the assets
        for (const auto& asset : m_EngineAssets)
        {
            if (!asset.Get())
            {
                continue;
            }

            bool reloadRequired = false;

            for (const auto& source : asset.Get()->GetSources())
            {
                if (!std::filesystem::exists(source.FilePath))
                {
                    continue;
                }

                auto currentWriteTime = std::filesystem::last_write_time(source.FilePath).time_since_epoch().count();
                if (currentWriteTime != source.LastWriteTime)
                {
                    reloadRequired = true;
                }
            }

            if (!reloadRequired)
            {
                continue;
            }

            asset.Get()->ReImport();

            reloadedAssets++;
        }

        if (reloadedAssets > 0)
        {
            Pine::Log::Info(fmt::format("Reloaded {} assets due to hot-reload", reloadedAssets));
        }
    }

    void OnWindowFocusCallback(GLFWwindow*, int focused)
    {
        if (!focused)
        {
            return;
        }

        UpdateAssets();
    }
}

void Pine::Utilities::HotReload::Setup()
{
    if (!Engine::GetEngineConfiguration().m_EnableDebugTools)
    {
        return;
    }

    for (const auto& [id, asset] : Assets::GetAll())
    {
        m_EngineAssets.emplace_back(asset->GetUId());
    }

    glfwSetWindowFocusCallback(static_cast<GLFWwindow*>(WindowManager::GetWindowPointer()), OnWindowFocusCallback);
}

void Pine::Utilities::HotReload::Shutdown()
{
}