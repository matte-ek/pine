#include "RenderHandler.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace
{
    Pine::Graphics::IFrameBuffer* m_RenderFrameBuffer = nullptr;

    void OnPineRender(Pine::RenderStage stage)
    {
        if (stage == Pine::RenderStage::PreRender)
        {
        }
    }
}

void RenderHandler::Setup()
{
    Pine::RenderManager::AddRenderCallback(OnPineRender);

    m_RenderFrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_RenderFrameBuffer->Create(1920, 1080, Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::Buffers::DepthBuffer | Pine::Graphics::Buffers::NormalBuffer);
}

void RenderHandler::Shutdown()
{
    m_RenderFrameBuffer->Dispose();

    delete m_RenderFrameBuffer;
}

Pine::Graphics::IFrameBuffer* RenderHandler::GetRenderFrameBuffer()
{
    return m_RenderFrameBuffer;
}