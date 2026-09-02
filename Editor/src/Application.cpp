#include <filesystem>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Script/ScriptManager.hpp"
#include "Pine/World/World.hpp"

#include "Gui/Gui.hpp"
#include "Other/EditorEntity/EditorEntity.hpp"
#include "Pine/Utilities/HotReload/HotReload.hpp"
#include "Projects/Projects.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Utilities/Scripts/ScriptUtilities.hpp"

int main(int argc, const char* argv[])
{
    // Get project name
    if (argc < 2)
    {
        PFatal("Usage: Editor <project_name>");
        return 1;
    }

    // Setup Pine
    Pine::Engine::EngineConfiguration engineConfiguration;

    engineConfiguration.m_WindowTitle = "Pine Engine Editor";
    engineConfiguration.m_WindowPosition = Pine::Vector2i(20, 20);
    engineConfiguration.m_WindowSize = Pine::Vector2i(1920, 1080);
    engineConfiguration.m_ProductionMode = false;
    engineConfiguration.m_WindowUseX11 = std::getenv("PINE_X11") != nullptr;

    if (!Pine::Engine::Setup(engineConfiguration))
    {
        return 0;
    }

    // Load editor assets
    Pine::Assets::LoadAssetsFromDirectory("editor");
    
    Editor::Projects::SetProject(argv[1]);

    // Load user assets
    Editor::Projects::LoadProjectAssets();

    // Now that the project and its assets are loaded, load the project's C# game assembly
    // (built externally by the user's IDE) so scripts can be resolved against it.
    const auto gameAssemblyPath = Editor::Projects::GetProjectPath() + "/runtime-bin/Game.dll";
    if (std::filesystem::exists(gameAssemblyPath))
    {
        Pine::Script::Manager::LoadGameAssembly(gameAssemblyPath);
    }
    else
    {
        PWarning(fmt::format("No game assembly found at '{}', scripts will be unavailable until the project is built.", gameAssemblyPath));
    }

    // Make sure we're not starting simulation
    Pine::World::SetPaused(true);

    // Setup Editor
    Editor::LevelEntity::Setup();
    Editor::RenderHandler::Setup();
    Editor::Gui::Setup();
    Editor::Utilities::Script::Setup();

    // Enter main loop
    Pine::Engine::Run();

    // Editor clean up
    Editor::Gui::Shutdown();
    Editor::RenderHandler::Shutdown();
    Editor::LevelEntity::Dispose();

    // Engine clean up
    Pine::Engine::Shutdown();

    return 0;
}
