#include <Pine/Pine.hpp>

int main()
{
    Pine::Engine::EngineConfiguration engineConfiguration;

    engineConfiguration.m_WindowTitle = "Pine Game Host";
    engineConfiguration.m_WindowSize = Pine::Vector2i(1920, 1080);

    if (!Pine::Engine::Setup(engineConfiguration))
    {
        return 0;
    }

    Pine::Engine::Run();

    Pine::Engine::Shutdown();

    return 0;
}