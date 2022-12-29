#include "Graphics.hpp"

#include "OpenGL/OpenGL.hpp"

namespace
{
    Pine::Graphics::IGraphicsAPI* m_GraphicsAPI = nullptr;
}

bool Pine::Graphics::Setup(Pine::Graphics::GraphicsAPI api)
{
    switch (api)
    {
        case GraphicsAPI::OpenGL:
            m_GraphicsAPI = new OpenGL();
            break;
        case GraphicsAPI::Vulkan:
            // Not supported for now.
            return false;
        default:
            return false;
    }

    return m_GraphicsAPI->Setup();
}

void Pine::Graphics::Shutdown()
{
    if (m_GraphicsAPI == nullptr)
    {
        return;
    }

    m_GraphicsAPI->Shutdown();

    delete m_GraphicsAPI;
}

Pine::Graphics::IGraphicsAPI *Pine::Graphics::GetGraphicsAPI()
{
    return m_GraphicsAPI;
}

bool Pine::Graphics::HasInitializedGraphicsAPI()
{
    return m_GraphicsAPI != nullptr;
}