#include "QuadTarget.hpp"

#include <vector>

#include "Pine/Graphics/Graphics.hpp"

namespace Pine::Graphics
{
    class IVertexArray;
}

namespace
{
    Pine::Graphics::IVertexArray* m_QuadVertexArray = nullptr;
}

void Pine::Rendering::Common::QuadTarget::Setup()
{
    const std::vector quads =
    {
        -1.f, 1.f, 0.f,
        -1.f, -1.f, 0.f,
        1.f, -1.f, 0.f,
        1.f, 1.f, 0.f,
    };

    const std::vector<std::uint32_t> indices =
    {
        0, 1, 3,
        3, 1, 2
    };

    const std::vector uvs =
    {
        0.f, 0.f,
        0.f, 1.f,
        1.f, 1.f,
        1.f, 0.f
    };

    m_QuadVertexArray = Graphics::GetGraphicsAPI()->CreateVertexArray();
    m_QuadVertexArray->Bind();

    m_QuadVertexArray->StoreFloatArrayBuffer(const_cast<float*>(quads.data()), quads.size() * sizeof(float), 0, 3, Graphics::BufferUsageHint::StaticDraw);
    m_QuadVertexArray->StoreFloatArrayBuffer(const_cast<float*>(uvs.data()), uvs.size() * sizeof(float), 1, 2, Graphics::BufferUsageHint::StaticDraw);
    m_QuadVertexArray->StoreElementArrayBuffer(const_cast<std::uint32_t*>(indices.data()), indices.size() * sizeof(int));
}

void Pine::Rendering::Common::QuadTarget::Shutdown()
{
    Graphics::GetGraphicsAPI()->DestroyVertexArray(m_QuadVertexArray);
}

void Pine::Rendering::Common::QuadTarget::Render()
{
    m_QuadVertexArray->Bind();

    Graphics::GetGraphicsAPI()->DrawElements(Graphics::RenderMode::Triangles, 12);
}
