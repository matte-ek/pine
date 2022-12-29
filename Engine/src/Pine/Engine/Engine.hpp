#pragma once
#include <Pine/Core/Math/Math.hpp>
#include <Pine/Graphics/Graphics.hpp>

#include <string>

namespace Pine::Engine
{

    struct EngineConfiguration
    {
        // Window setup
        Vector2i m_WindowPosition = Vector2i(-1); // -1 will center the window.
        Vector2i m_WindowSize = Vector2i(1024, 1024);
        std::string m_WindowTitle = "Pine";

        // The maximum amount of threads the asset manager may use while
        // loading assets from a directory.
        int m_AssetsLoadThreadCount = 4;

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
