#include <Pine/Pine.hpp>

#include <filesystem>

#include "Pine/Rendering/RenderingContext.hpp"
#include "Pine/Rendering/Renderer2D/Renderer2D.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Script/ScriptManager.hpp"

namespace
{

    Pine::Texture2D* text = nullptr;

    void OnPineRender(Pine::RenderingContext* context, Pine::RenderStage stage, float deltaTime)
    {
        if (context == nullptr)
        {
            return;
        }

        Pine::Renderer2D::PrepareFrame();

        Pine::Renderer2D::SetCoordinateSystem(Pine::Rendering::CoordinateSystem::Screen);

        Pine::Renderer2D::AddFilledTexturedRectangle(
            Pine::Vector2f(0.f, 0.f),
            Pine::Vector2f(256.f, 256.f),
            0.f,
            Pine::Color::Red,
            text);

        Pine::Renderer2D::RenderFrame(context);
    }

}

int main()
{
    Pine::Engine::EngineConfiguration engineConfiguration;

    engineConfiguration.m_WindowTitle = "Pine Game Host";
    engineConfiguration.m_WindowSize = Pine::Vector2i(1920, 1080);

    if (!Pine::Engine::Setup(engineConfiguration))
    {
        return 0;
    }

    // The standalone host runs a single baked game from data/game/.
    if (std::filesystem::exists("game/runtime-bin/Game.dll"))
    {
        Pine::Script::Manager::LoadGameAssembly("game/runtime-bin/Game.dll");
    }

    text = Pine::Assets::Get<Pine::Texture2D>("game/assets/test");

    Pine::RenderManager::AddRenderCallback(OnPineRender);

    Pine::Engine::Run();

    Pine::Engine::Shutdown();

    return 0;
}