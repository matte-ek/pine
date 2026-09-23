#include <Pine/Pine.hpp>

#include <cstdlib>
#include <filesystem>

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Game/Game.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Script/ScriptManager.hpp"

namespace
{

    // The gameplay assembly is built next to the assets it belongs to, so the game file only has to
    // record one path. While developing that resolves to the editor project's runtime-bin/Game.dll.
    std::filesystem::path GetGameAssemblyPath(const std::filesystem::path& assetRoot)
    {
        return assetRoot.parent_path() / "runtime-bin" / "Game.dll";
    }

    // The host renders straight into the window rather than into a framebuffer the way the editor's
    // viewports do, so the context the game is drawn through has to follow the window's size.
    void ResizeGameView(const Pine::Vector2i& size)
    {
        Pine::RenderManager::GetPrimaryRenderingContext()->Size = Pine::Vector2f(size);
    }

}

int main()
{
    Pine::Engine::EngineConfiguration engineConfiguration;

    engineConfiguration.m_WindowTitle = "Pine Game Host";
    engineConfiguration.m_WindowSize = Pine::Vector2i(1920, 1080);
    engineConfiguration.m_WindowUseX11 = std::getenv("PINE_X11") != nullptr;
    engineConfiguration.m_VerboseLogging = std::getenv("PINE_VERBOSE") != nullptr;

    if (!Pine::Engine::Setup(engineConfiguration))
    {
        return 1;
    }

    // Engine::Setup has read game/game.json by now, which is what tells us which game to run.
    const auto& gameProperties = Pine::Game::GetGameProperties();

    if (gameProperties.AssetRoot.empty())
    {
        PFatal("The game file specifies no asset root, so there is nothing to run. Save the game "
               "properties from the editor to write one.");

        Pine::Engine::Shutdown();

        return 1;
    }

    // Load the game's assets under the same virtual paths the editor used, so the level, material
    // and script references stored inside them still resolve.
    Pine::Assets::SetWorkingDirectory(gameProperties.AssetRoot);

    if (Pine::Assets::LoadAssetsFromDirectory("") <= 0)
    {
        PFatal(fmt::format("No game assets were found in '{}'.", gameProperties.AssetRoot));

        Pine::Engine::Shutdown();

        return 1;
    }

    // Has to happen before Run(), which is where the script manager resolves every script component
    // against the assembly.
    const auto gameAssemblyPath = GetGameAssemblyPath(gameProperties.AssetRoot);

    if (std::filesystem::exists(gameAssemblyPath))
    {
        Pine::Script::Manager::LoadGameAssembly(gameAssemblyPath.string());
    }
    else
    {
        PWarning(fmt::format("No game assembly found at '{}', scripts will not run.", gameAssemblyPath.string()));
    }

    if (!gameProperties.Name.empty())
    {
        Pine::WindowManager::SetWindowTitle(gameProperties.Name);
    }

    // The window may already differ from the size we asked for, so take the real one before the
    // first frame instead of waiting for a resize that might never come.
    ResizeGameView(Pine::WindowManager::GetWindowSize());

    Pine::WindowManager::AddWindowResizeCallback([](const int width, const int height)
    {
        ResizeGameView(Pine::Vector2i(width, height));
    });

    // Run() loads the startup level named in the game file and then enters the main loop.
    Pine::Engine::Run();

    Pine::Engine::Shutdown();

    return 0;
}
