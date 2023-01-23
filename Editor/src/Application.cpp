#include "Gui/Gui.hpp"
#include "Rendering/RenderHandler.hpp"
#include <Pine/Pine.hpp>

int main()
{
    // Setup Pine
    Pine::Engine::EngineConfiguration engineConfiguration;

    engineConfiguration.m_WindowTitle = "Pine Engine Editor";
    engineConfiguration.m_WindowSize = Pine::Vector2i(1600, 900);

    if (!Pine::Engine::Setup(engineConfiguration))
    {
        return 0;
    }

    // Setup Editor
    RenderHandler::Setup();
    Gui::Setup();

    // Enter main loop
    Pine::Engine::Run();

    // Editor clean up
    Gui::Shutdown();
    RenderHandler::Shutdown();

    // Engine clean up
    Pine::Engine::Shutdown();

    return 0;
}
