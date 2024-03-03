#pragma once
#include <Pine/Core/Math/Math.hpp>
#include <Pine/Graphics/Graphics.hpp>
#include <Pine/Audio/Audio.hpp>

#include <string>

namespace Pine::Engine
{

    struct EngineConfiguration
    {
        // Window setup
        Vector2i m_WindowPosition = Vector2i(-1); // -1 will center the window.
        Vector2i m_WindowSize = Vector2i(1280, 720);
        std::string m_WindowTitle = "Pine Engine";

        // The maximum amount of threads the asset manager may use while
        // loading assets from a directory.
        int m_AssetsLoadThreadCount = 4;

        // Refers to both entity and component count
        std::uint32_t m_MaxObjectCount = 2048;

        // Whether to enable engine debug tools, such as hot reload
        bool m_EnableDebugTools = true;

        // Whether to pause and wait for new events instead of rendering a new frame instantly.
        bool m_WaitEvents = false;

        // Disables all the engine stuff (such as handling entities, input and physics)
        bool m_Standalone = false;

        Graphics::GraphicsAPI m_GraphicsAPI = Graphics::GraphicsAPI::OpenGL;
    };

    // Attempts to set up the engine with provided engine configuration,
    // will also create a default window, and therefore graphics context.
    // Returns false on failure.
    bool Setup(const EngineConfiguration& engineConfiguration = EngineConfiguration());

    // Starts all threads and enters the engine's main rendering loop.
    // Execution will be blocked until exit. Make sure Setup() has been
    // called successfully.
    void Run();

    // Cleans up resources and gracefully shutdowns everything.
    // Call after Run(), before exiting the application.
    void Shutdown();

    // Returns true if Setup() has been called and completed successfully
    bool IsInitialized();

    // Returns the engine configuration used during Setup(), values such as
    // window parameters are not updated.
    const EngineConfiguration& GetEngineConfiguration();

}
