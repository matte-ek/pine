#include "HotReload.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Log/Log.hpp"
#include <GLFW/glfw3.h>

namespace
{
    void UpdateAssets()
    {
        Pine::Log::Verbose("HotReload: Reloading assets due to window focus.");

        Pine::Assets::LoadDirectory("engine/shaders", false);
        Pine::Assets::LoadDirectory("engine", false);
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

    glfwSetWindowFocusCallback(static_cast<GLFWwindow*>(WindowManager::GetWindowPointer()), OnWindowFocusCallback);
}

void Pine::Utilities::HotReload::Shutdown()
{
}