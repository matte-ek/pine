#pragma once
#include <string>

namespace Pine::Game
{
    struct GameProperties
    {
        // Meta
        std::string Name;
        std::string Version;
        std::string Author;

        // Startup
        std::string StartupLevel;
    };

    const GameProperties& GetGameProperties();

    void Setup();
    void OnStartup();
}
