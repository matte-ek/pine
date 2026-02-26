#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/World/World.hpp"

#include "Gui/Gui.hpp"
#include "Other/EditorEntity/EditorEntity.hpp"
#include "Projects/Projects.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Utilities/Scripts/ScriptUtilities.hpp"

int main(int argc, const char* argv[])
{
    // Get project name
    if (argc < 2)
    {
        Pine::Log::Fatal("Usage: Editor <project_name>");
        return 1;
    }

    // Setup Pine
    Pine::Engine::EngineConfiguration engineConfiguration;

    engineConfiguration.m_WindowTitle = "Pine Engine Editor";
    engineConfiguration.m_WindowPosition = Pine::Vector2i(20, 20);
    engineConfiguration.m_WindowSize = Pine::Vector2i(1920, 1080);
    engineConfiguration.m_ProductionMode = false;

    if (!Pine::Engine::Setup(engineConfiguration))
    {
        return 0;
    }

    // Load editor assets
    Pine::Assets::LoadAssetsFromDirectory("editor");

    Editor::Projects::SetProject(argv[1]);

    // Load user assets
    Editor::Projects::LoadProjectAssets();

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
