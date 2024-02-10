#include "ScriptReload.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Script/ScriptManager.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include <filesystem>

namespace
{

    constexpr auto GameAssemblyPath = "game/runtime-bin/Game.dll";

    std::filesystem::file_time_type m_LastGameAssemblyWriteTime;
    bool m_ReloadGameAssemblyRequest = false;

    bool PollGameAssemblyWriteTime()
    {
        const auto writeTime = std::filesystem::last_write_time(GameAssemblyPath);

        if (writeTime != m_LastGameAssemblyWriteTime)
        {
            m_LastGameAssemblyWriteTime = writeTime;

            return true;
        }

        return false;
    }

    void OnWindowFocus()
    {
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
            return;
        if (!PollGameAssemblyWriteTime())
            return;

        m_ReloadGameAssemblyRequest = true;
    }

    void OnRender(Pine::RenderingContext*, Pine::RenderStage stage, float)
    {
        if (stage != Pine::RenderStage::PostRender)
            return;

        if (m_ReloadGameAssemblyRequest)
        {
            Pine::Log::Message("Reloading updated game assembly...");

            Pine::Script::Manager::ReloadGameAssembly();

            m_ReloadGameAssemblyRequest = false;
        }
    }

}

void ScriptReload::Setup()
{
    Pine::WindowManager::AddWindowFocusCallback(OnWindowFocus);
    Pine::RenderManager::AddRenderCallback(OnRender);

    PollGameAssemblyWriteTime();
}
