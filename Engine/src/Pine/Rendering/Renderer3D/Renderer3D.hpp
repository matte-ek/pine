#pragma once

#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"

namespace Pine::Renderer3D
{
    void Setup();
    void Shutdown();

    void PrepareMesh(Mesh* mesh);
    void RenderMesh(const Matrix4f& transformationMatrix);

    void SetCamera(Pine::Camera* camera);
    Pine::Camera* GetCamera();

    void SetShader(Pine::Shader* shader);
    Pine::Shader* GetShader();
}