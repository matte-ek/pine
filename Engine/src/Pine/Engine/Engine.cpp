#include "Engine.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Audio/Audio.hpp"
#include "Pine/Graphics/TextureAtlas/TextureAtlas.hpp"
#include "Pine/Input/Input.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Utilities/HotReload/HotReload.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Pine/Script/ScriptManager.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>

#include "Pine/Game/Game.hpp"
#include "Pine/Physics/Physics2D/Physics2D.hpp"
#include "Pine/Threading/Threading.hpp"

namespace
{
    bool m_IsInitialized = false;

    Pine::Engine::EngineConfiguration m_EngineConfiguration;
    Pine::Graphics::IGraphicsAPI* m_GraphicsAPI;
    Pine::Audio::IAudioAPI* m_AudioAPI;
}

bool Pine::Engine::Setup(const EngineConfiguration& engineConfiguration)
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

    Graphics::GetGraphicsAPI()->EnableErrorLogging();

    if (!Audio::Setup())
    {
        Log::Fatal("Failed to setup audio API");

        WindowManager::Internal::DestroyWindow();

        glfwTerminate();

        return false;
    }

    Threading::Setup();

    // We have to set up our script runtime first, since all other parts of the engine needs to
    // be able to register objects to the runtime.
    Script::Runtime::Setup();
    Script::Manager::Setup();

    // Load engine assets, order is important, we want the shaders ready
    // before the other stuff.

    Assets::Setup();

    if (Assets::LoadAssetsFromDirectory("data") <= 0)
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
    Renderer3D::Setup();
    Physics3D::Setup();
    Physics2D::Setup();
    Input::Setup();
    World::Setup();
    Game::Setup();
    Utilities::HotReload::Setup();

    // Finish initialization
    m_IsInitialized = true;
    m_GraphicsAPI = Graphics::GetGraphicsAPI();

    Log::Info("Pine was successfully initialized.");

    return true;
}

void Pine::Engine::Run()
{
    if (!m_IsInitialized)
    {
        throw std::runtime_error("Engine has not been initialized.");
    }

    const auto windowPointer = static_cast<GLFWwindow*>(WindowManager::GetWindowPointer());

    // During the window setup, we've actually made the window to be invisible by default.
    // This is because:
    // 1. Initialize the engine without making a visible frozen window.
    // 2. Allow the user to modify the window, without the window flickering during startup.
    // Therefore, we'll have to restore it here.
    WindowManager::SetWindowVisible(true);

    // At this point the user should have loaded their game assembly, so we can allow the
    // script manager to start preparing all the scripts.
    Script::Manager::ReloadScripts();

    // If a startup level has been specified, it's now time to load it before starting the main loop.
    Game::OnStart();

    // If we're in production mode (i.e. the game host), make sure to call all the "on start" things.
    if (m_EngineConfiguration.m_ProductionMode)
    {
        World::OnStart();
    }

    // The main rendering loop itself
    while (WindowManager::IsWindowOpen())
    {
        m_EngineConfiguration.m_WaitEvents ? glfwWaitEventsTimeout(1) : glfwPollEvents();

        m_GraphicsAPI->ClearColor(Color(0, 0, 0, 255));
        m_GraphicsAPI->ClearBuffers(Graphics::ColorBuffer);

        if (!m_EngineConfiguration.m_Standalone)
        {
            Input::Update();
            World::Update();
        }

        RenderManager::Run();

        glfwSwapBuffers(windowPointer);
    }
}

void Pine::Engine::Shutdown()
{
    if (!m_IsInitialized)
    {
        throw std::runtime_error("Engine has not been initialized.");
    }

    Script::Manager::Dispose();
    Script::Runtime::Dispose();
    Utilities::HotReload::Shutdown();
    Input::Shutdown();
    Physics2D::Shutdown();
    Physics3D::Shutdown();
    Renderer3D::Shutdown();
    Entities::Shutdown();
    Components::Shutdown();
    RenderManager::Shutdown();
    Assets::Shutdown();
    Graphics::Shutdown();
    Audio::Shutdown();
    Threading::Shutdown();

    WindowManager::Internal::DestroyWindow();

    glfwTerminate();
}

bool Pine::Engine::IsInitialized()
{
    return m_IsInitialized;
}

Pine::Engine::EngineConfiguration &Pine::Engine::GetEngineConfiguration()
{
    return m_EngineConfiguration;
}