#include "Renderer3D.hpp"

#include "Specifications.hpp"
#include "ShaderStorages.hpp"
#include "Pine/Core/Log/Log.hpp"

namespace
{
    Pine::Graphics::IGraphicsAPI* m_GraphicsAPI = nullptr;

    Pine::Shader* m_Shader = nullptr;
    Pine::Graphics::IUniformVariable* m_TransformationMatrix = nullptr;

    Pine::Mesh* m_Mesh = nullptr;
    Pine::Material* m_Material = nullptr;

    Pine::Camera* m_Camera = nullptr;
}

void Pine::Renderer3D::Setup()
{
    m_GraphicsAPI = Graphics::GetGraphicsAPI();

    ShaderStorages::Matrix.Create();
    //ShaderStorages::Material.Create();
    //ShaderStorages::Lights.Create();
}

void Pine::Renderer3D::Shutdown()
{
    ShaderStorages::Matrix.Dispose();
    //ShaderStorages::Material.Dispose();
    //ShaderStorages::Lights.Dispose();
}

void Pine::Renderer3D::PrepareMesh(Pine::Mesh *mesh)
{
    mesh->GetVertexArray()->Bind();

    if (mesh->GetMaterial()->GetShader() != m_Shader)
        SetShader(mesh->GetMaterial()->GetShader());

    if (!m_Shader)
        return;

    m_Mesh = mesh;
    m_Material = mesh->GetMaterial();
}

void Pine::Renderer3D::RenderMesh(const Pine::Matrix4f &transformationMatrix)
{
    m_TransformationMatrix->LoadMatrix4(transformationMatrix);

    if (m_Mesh->HasElementBuffer())
    {
        m_GraphicsAPI->DrawElements(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount());
    }
    else
    {
        m_GraphicsAPI->DrawArrays(Graphics::RenderMode::Triangles, m_Mesh->GetRenderCount());
    }
}

void Pine::Renderer3D::SetShader(Pine::Shader *shader)
{
    if (!shader->IsReady())
    {
        if (!ShaderStorages::Matrix.AttachShader(shader))
        {
            Log::Error("Shader is missing 'Matrix' shader storage, expect rendering issues.");
        }

        //ShaderStorages::Material.AttachShader(shader);
        //ShaderStorages::Lights.AttachShader(shader);

        shader->SetReady(true);
    }

    m_TransformationMatrix = shader->GetProgram()->GetUniformVariable("m_TransformationMatrix");
    m_Shader = shader;

    m_Shader->GetProgram()->Use();

    assert(m_TransformationMatrix);
}

Pine::Shader *Pine::Renderer3D::GetShader()
{
    return m_Shader;
}

void Pine::Renderer3D::SetCamera(Pine::Camera *camera)
{
    m_Camera = camera;

    ShaderStorages::Matrix.Data().Projection = camera->GetProjectionMatrix();
    ShaderStorages::Matrix.Data().View = camera->GetViewMatrix();

    ShaderStorages::Matrix.Upload();
}

Pine::Camera *Pine::Renderer3D::GetCamera()
{
    return m_Camera;
}
