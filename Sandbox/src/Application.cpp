#include <Pine/Pine.hpp>
#include "Pine/Rendering/RenderingContext.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Rendering/Renderer2D/Renderer2D.hpp"
#include "Pine/Assets/Font/Font.hpp"

namespace
{
    Pine::Font* font = nullptr;
    std::uint32_t fontSize12 = 0;
}

void OnPineRender(Pine::RenderingContext*, Pine::RenderStage stage, float deltaTime)
{
    if (stage != Pine::RenderStage::Render2D)
        return;

    Pine::RenderingContext context {true, true, Pine::Vector2f(1280, 720) };

    Pine::Renderer2D::PrepareFrame();
    Pine::Renderer2D::SetCoordinateSystem(Pine::Rendering::CoordinateSystem::Screen);

    // För att rendera en fylld rektangel:
    Pine::Renderer2D::AddFilledRectangle(Pine::Vector2f(100.f, 100.f), Pine::Vector2f(100.f, 100.f), 0.f, Pine::Color(255, 0, 0));

    // För att rendera en fylld rektangel med en textur:
    // Renderer2D::AddFilledTexturedRectangle(Vector2f position, Vector2f size, float rotation, Color color, const Texture2D* texture, Vector2f uvOffset, Vector2f uvScale)

    // I början bör stb_truetype.h kunna generara en texture atlas med alla tecken som behövs, som kan laddas in som en vanlig textur.
    // Denna textur kan sedan användas för att rendera text.

    const auto& fontData = font->GetFontData(fontSize12);

    Pine::Renderer2D::AddFilledTexturedRectangle(Pine::Vector2f(200.f, 200.f), Pine::Vector2f(1024.f, 1024.f), 0.f, Pine::Color(255, 255, 255), fontData.m_TextureFontAtlas);

    // Till slut bör text kunna renderas så här
    //Pine::Renderer2D::AddText(Pine::Vector2f(100.f, 100.f), Pine::Color(255, 255, 255), font, "Hello world!");

    Pine::Renderer2D::RenderFrame(&context);
}

int main()
{
    Pine::Engine::EngineConfiguration engineConfiguration;

    engineConfiguration.m_WindowSize = Pine::Vector2i(1280, 720);

    if (!Pine::Engine::Setup(engineConfiguration))
    {
        return 0;
    }

    Pine::Assets::LoadDirectory("game");
    Pine::RenderManager::AddRenderCallback(OnPineRender);

    font = Pine::Assets::Get<Pine::Font>("NotoSans-Regular.ttf");
    fontSize12 = font->Create(18.f);

    Pine::Engine::Run();

    Pine::Engine::Shutdown();

    return 0;
}