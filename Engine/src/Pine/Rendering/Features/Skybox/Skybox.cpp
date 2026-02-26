#include "Skybox.hpp"
#include "Pine/Graphics/Interfaces/IVertexArray.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Renderer3D/ShaderStorages.hpp"

namespace
{
    Pine::Graphics::IGraphicsAPI* m_GraphicsAPI = nullptr;

    Pine::Graphics::IVertexArray* m_VertexArray = nullptr;
    Pine::Shader* m_Shader = nullptr;
}

void Pine::Rendering::Skybox::Setup()
{
    const std::vector<float> skyboxVertices = { -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
                                                1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

                                                -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
                                                -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

                                                1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
                                                1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

                                                -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
                                                1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

                                                -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
                                                1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

                                                -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
                                                1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f };

    m_GraphicsAPI = Graphics::GetGraphicsAPI();

    m_VertexArray = m_GraphicsAPI->CreateVertexArray();
    m_VertexArray->Bind();
    m_VertexArray->StoreFloatArrayBuffer(const_cast<float*>(skyboxVertices.data()), skyboxVertices.size() * sizeof(float), 0, 3, Pine::Graphics::BufferUsageHint::StaticDraw);

    m_Shader = Pine::Assets::Get<Pine::Shader>("engine/shaders/3d/skybox");

    assert(m_Shader != nullptr);
}

void Pine::Rendering::Skybox::Shutdown()
{
    m_GraphicsAPI->DestroyVertexArray(m_VertexArray);
}

void Pine::Rendering::Skybox::Render(Texture3D* cubeMap)
{
    PINE_PF_SCOPE();

    if (!cubeMap->IsReady())
    {
        cubeMap->Build();
        return;
    }

    if (!m_Shader->IsRendererReady())
    {
        assert(Pine::Renderer3D::ShaderStorages::Matrix.AttachShaderProgram(m_Shader->GetProgram()));
        m_Shader->SetRendererReady(true);
    }

    m_Shader->GetProgram()->Use();
    m_VertexArray->Bind();
    cubeMap->GetCubeMap()->Bind();

    m_GraphicsAPI->SetDepthFunction(Graphics::TestFunction::LessEqual);
    m_GraphicsAPI->DrawArrays(Graphics::RenderMode::Triangles, 36);
    m_GraphicsAPI->SetDepthFunction(Graphics::TestFunction::Less);
}
