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

        // Where the game's assets live, relative to the directory the game is run from
        // (while developing, that's an editor project, e.g. "projects/gm/assets"). The
        // standalone host points the asset manager here before loading anything, so the
        // virtual paths stored in the game file and in the assets themselves resolve to
        // the same thing they did in the editor.
        std::string AssetRoot;

        // The level that will be loaded upon starting the game,
        // could be a loading scene, or the main menu etc.
        std::string StartupLevel;
    };

    void SetGameProperties(const GameProperties& gameProperties);
    const GameProperties& GetGameProperties();

    void Setup();
    void OnStart();
}
