#include "ScriptUtilities.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Script/ScriptManager.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Projects/Projects.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    // Template for a fresh gameplay script. The class inherits Pine.World.Components.Script and
    // lives in the "Game" namespace, matching how the script manager resolves managed types.
    constexpr auto ScriptTemplate = R"(using Pine.World.Components;

namespace Game
{
    public class %CLASS% : Script
    {
        public void OnStart()
        {
        }

        public void OnUpdate(float deltaTime)
        {
        }
    }
}
)";

    std::filesystem::file_time_type m_LastGameAssemblyWriteTime;
    bool m_ReloadGameAssemblyRequest = false;

    // The game assembly is per-project: <project>/runtime-bin/Game.dll (relative to the working
    // directory, i.e. data/). Built externally by the user's IDE.
    std::string GetGameAssemblyPath()
    {
        return Editor::Projects::GetProjectPath() + "/runtime-bin/Game.dll";
    }

    bool PollGameAssemblyWriteTime()
    {
        const auto path = GetGameAssemblyPath();

        if (!std::filesystem::exists(path))
            return false;

        const auto writeTime = std::filesystem::last_write_time(path);

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
            PInfo("Reloading updated game assembly...");

            Pine::Script::Manager::ReloadGameAssembly();

            m_ReloadGameAssemblyRequest = false;
        }
    }
}

void Editor::Utilities::Script::Setup()
{
    Pine::WindowManager::AddWindowFocusCallback(OnWindowFocus);
    Pine::RenderManager::AddRenderCallback(OnRender);

    PollGameAssemblyWriteTime();
}

void Editor::Utilities::Script::CreateScriptSource(const std::string& csFilePath, const std::string& className)
{
    std::string source = ScriptTemplate;

    for (auto pos = source.find("%CLASS%"); pos != std::string::npos; pos = source.find("%CLASS%"))
    {
        source.replace(pos, std::string("%CLASS%").length(), className);
    }

    std::ofstream stream(csFilePath, std::ios::out | std::ios::trunc);
    if (!stream.is_open())
    {
        PError(fmt::format("Failed to create script source file: {}", csFilePath));
        return;
    }

    stream << source;
}

void Editor::Utilities::Script::DeleteScript(const std::string& passetFilePath)
{
    auto csPath = std::filesystem::path(passetFilePath).replace_extension(".cs");

    std::error_code ec;
    std::filesystem::remove(csPath, ec);
}
