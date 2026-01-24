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

        // Game specific data
        std::string EntityTags[64];
        std::string ColliderLayers[31];

        // The level that will be loaded upon starting the game,
        // could be a loading scene, or the main menu etc.
        std::string StartupLevel;
    };

    void SetGameProperties(const GameProperties& gameProperties);
    const GameProperties& GetGameProperties();

    void Setup();
    void OnStart();
}
