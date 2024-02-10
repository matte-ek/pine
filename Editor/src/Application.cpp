#include "Gui/Gui.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Rendering/RenderHandler.hpp"
#include <Pine/Pine.hpp>

#include "Other/EditorEntity/EditorEntity.hpp"
#include "Pine/World/World.hpp"
#include "Scripting/ScriptReload.hpp"

int main()
{
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

    Pine::Assets::LoadDirectory("editor", false);
    Pine::Assets::LoadDirectory("game/assets");
    Pine::World::SetPaused(true);

    // Setup Editor
    EditorEntity::Setup();
    RenderHandler::Setup();
    Gui::Setup();
    ScriptReload::Setup();

    // Enter main loop
    Pine::Engine::Run();

    // Editor clean up
    Gui::Shutdown();
    RenderHandler::Shutdown();
    EditorEntity::Dispose();

    // Engine clean up
    Pine::Engine::Shutdown();


    return 0;
}
