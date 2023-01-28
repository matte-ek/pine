#include "Engine.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/TextureAtlas/TextureAtlas.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Entities/Entities.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>


namespace
{
    bool m_IsInitialized = false;

    Pine::Engine::EngineConfiguration m_EngineConfiguration;
    Pine::Graphics::IGraphicsAPI* m_GraphicsAPI;
}

bool Pine::Engine::Setup(const Pine::Engine::EngineConfiguration& engineConfiguration)
{
    m_EngineConfiguration = engineConfiguration;

    // Initially we need to initialize some core stuff, such as libraries
    // and a window (therefore graphics context) before initializing the rest
    // of the engine.

    if (!glfwInit())
    {
        Log::Fatal("Failed to setup core library: GLFW");

        return false;
    }

    if (!WindowManager::Internal::CreateWindow(engineConfiguration.m_WindowPosition, engineConfiguration.m_WindowSize,
                                               engineConfiguration.m_WindowTitle, WindowManager::ScreenType::Default))
    {
        Log::Fatal("Failed to setup window");

        glfwTerminate();

        return false;
    }

    // Set up our graphics API.
    if (!Graphics::Setup(engineConfiguration.m_GraphicsAPI))
    {
        Log::Fatal("Failed to setup graphics API");

        WindowManager::Internal::DestroyWindow();

        glfwTerminate();

        return false;
    }

    // Load engine assets, order is important, we want the shaders ready
    // before the other stuff.

    Assets::Setup();

    if (Assets::LoadDirectory("engine/shaders", false) != 0
     || Assets::LoadDirectory("engine", false) != 0)
    {
        Log::Fatal("Failed to load engine assets.");

        WindowManager::Internal::DestroyWindow();

        glfwTerminate();

        return false;
    }

    // At this point we should be safe to start initializing parts of the engine
    Components::Setup();
    Entities::Setup();
    RenderManager::Setup();

    // Finish initialization
    m_IsInitialized = true;
    m_GraphicsAPI = Graphics::GetGraphicsAPI();

    Log::Message("Pine was successfully initialized.");

    return true;
}

void Pine::Engine::Run()
{
    if (!m_IsInitialized)
    {
        throw std::runtime_error("Engine::Run(): Engine has not been initialized.");
    }

    const auto windowPointer = WindowManager::GetWindowPointer();

    // During the window setup, we've actually made the window to be invisible by default.
    // This is because:
    // 1. Initialize the engine without making a visible frozen window.
    // 2. Allow the user to modify the window, without the window flickering during startup.
    // So we'll have to restore it here.
    WindowManager::SetWindowVisible(true);

    // The main rendering loop itself
    while (WindowManager::IsWindowOpen())
    {
        glfwPollEvents();

        m_GraphicsAPI->ClearColor(Color(0, 0, 0, 255));
        m_GraphicsAPI->ClearBuffers(Graphics::ColorBuffer);

        RenderManager::Run();

        glfwSwapBuffers(windowPointer);
    }
}

void Pine::Engine::Shutdown()
{
    if (!m_IsInitialized)
    {
        throw std::runtime_error("Engine::Shutdown(): Engine has not been initialized.");
    }

    Entities::Shutdown();
    Components::Shutdown();
    RenderManager::Shutdown();
    Assets::Shutdown();
    Graphics::Shutdown();

    WindowManager::Internal::DestroyWindow();

    glfwTerminate();
}

bool Pine::Engine::IsInitialized()
{
    return m_IsInitialized;
}

const Pine::Engine::EngineConfiguration &Pine::Engine::GetEngineConfiguration()
{
    return m_EngineConfiguration;
}