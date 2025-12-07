#include "Skybox.hpp"
#include "Pine/Graphics/Interfaces/IVertexArray.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Assets/Assets.hpp"
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

    m_GraphicsAPI = Pine::Graphics::GetGraphicsAPI();

    m_VertexArray = m_GraphicsAPI->CreateVertexArray();
    m_VertexArray->Bind();
    m_VertexArray->StoreFloatArrayBuffer(const_cast<float*>(skyboxVertices.data()), skyboxVertices.size() * sizeof(float), 0, 3, Pine::Graphics::BufferUsageHint::StaticDraw);

    m_Shader = Pine::Assets::Get<Pine::Shader>("engine/shaders/3d/skybox.shader");
}

void Pine::Rendering::Skybox::Shutdown()
{
    m_GraphicsAPI->DestroyVertexArray(m_VertexArray);
}

void Pine::Rendering::Skybox::Render(const Texture3D* cubeMap)
{
    if (!cubeMap->IsValid())
    {
        return;
    }

    if (!m_Shader->IsReady())
    {
        assert(Pine::Renderer3D::ShaderStorages::Matrix.AttachShaderProgram(m_Shader->GetProgram()));

        m_Shader->SetReady(true);
    }

    m_Shader->GetProgram()->Use();
    m_VertexArray->Bind();
    cubeMap->GetCubeMap()->Bind();

    m_GraphicsAPI->SetDepthFunction(Graphics::TestFunction::LessEqual);

    m_GraphicsAPI->DrawArrays(Graphics::RenderMode::Triangles, 36);

    m_GraphicsAPI->SetDepthFunction(Graphics::TestFunction::Less);
}
