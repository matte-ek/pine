#include "RenderHandler.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

#include "Gui/Panels/GameViewport/GameViewportPanel.hpp"
#include "Gui/Panels/LevelViewport/LevelViewportPanel.hpp"

namespace
{
    Pine::Graphics::IFrameBuffer* m_GameFrameBuffer = nullptr;
    Pine::Graphics::IFrameBuffer* m_LevelFrameBuffer = nullptr;

    Pine::RenderingContext* m_GameRenderingContext = nullptr;
    Pine::RenderingContext* m_LevelRenderingContext = nullptr;

    void OnPineRender(Pine::RenderStage stage)
    {
        if (!m_GameRenderingContext || !m_LevelRenderingContext)
            return;

        if (stage == Pine::RenderStage::PreRender)
        {
            m_GameRenderingContext->m_Active = Panels::GameViewport::GetActive() && Panels::GameViewport::GetVisible();
            m_GameRenderingContext->m_Size = Pine::Vector2f(1920, 1080);

            m_LevelRenderingContext->m_Active = Panels::LevelViewport::GetActive() && Panels::LevelViewport::GetVisible();
            m_LevelRenderingContext->m_Size = Pine::Vector2f(1920, 1080);
        }
    }
}

void RenderHandler::Setup()
{
    Pine::RenderManager::AddRenderCallback(OnPineRender);

    // Prepare frame buffers
    m_GameFrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_GameFrameBuffer->Create(1920, 1080, Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::Buffers::DepthBuffer);

    m_LevelFrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_LevelFrameBuffer->Create(1920, 1080, Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::Buffers::DepthBuffer);

    // Prepare rendering contexts
    m_GameRenderingContext = new Pine::RenderingContext;
    m_LevelRenderingContext = new Pine::RenderingContext;

    m_GameRenderingContext->m_FrameBuffer = m_GameFrameBuffer;
    m_LevelRenderingContext->m_FrameBuffer = m_LevelFrameBuffer;

    m_GameRenderingContext->m_ClearColor = Pine::Vector4f(1.f, 0.5f, 0.f, 1.f);
    m_LevelRenderingContext->m_ClearColor = Pine::Vector4f(0.f, 0.5f, 1.f, 1.f);

    // For the level rendering context, we'll always force the editor entity's camera
    m_LevelRenderingContext->m_Camera = nullptr;

    Pine::RenderManager::SetPrimaryRenderingContext(m_GameRenderingContext);
    Pine::RenderManager::AddRenderingContextPass(m_LevelRenderingContext);
}

void RenderHandler::Shutdown()
{
    m_GameFrameBuffer->Dispose();
    m_LevelFrameBuffer->Dispose();

    delete m_GameFrameBuffer;
    delete m_LevelFrameBuffer;

    delete m_GameRenderingContext;
    delete m_LevelRenderingContext;
}

Pine::Graphics::IFrameBuffer* RenderHandler::GetGameFrameBuffer()
{
    return m_GameFrameBuffer;
}

Pine::Graphics::IFrameBuffer* RenderHandler::GetLevelFrameBuffer()
{
    return m_LevelFrameBuffer;
}