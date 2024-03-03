#include <Pine/Pine.hpp>
#include "Pine/Rendering/RenderingContext.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Rendering/Renderer2D/Renderer2D.hpp"
#include "Pine/Assets/Font/Font.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Assets/Level/Level.hpp"

namespace
{
    float m_CurrentTime = 0.f;
    int m_Frames = 0;
}

void OnPineRender(Pine::RenderingContext*, Pine::RenderStage stage, float deltaTime)
{
    if (stage != Pine::RenderStage::PostRender)
        return;

    m_CurrentTime += deltaTime;
    m_Frames++;

    if (m_CurrentTime >= 1.f)
    {
        Pine::Log::Info(fmt::format("FPS: {}", m_Frames));
        m_CurrentTime = 0.f;
        m_Frames = 0;
    }
}

int main()
{
    Pine::Engine::EngineConfiguration engineConfiguration;

    engineConfiguration.m_WindowSize = Pine::Vector2i(1280, 720);

    if (!Pine::Engine::Setup(engineConfiguration))
    {
        return 0;
    }

    Pine::Assets::LoadDirectory("game/assets");
    Pine::RenderManager::AddRenderCallback(OnPineRender);

    Pine::Assets::Get<Pine::Level>("a.lvl")->Load();

    Pine::Engine::Run();

    Pine::Engine::Shutdown();

    return 0;
}