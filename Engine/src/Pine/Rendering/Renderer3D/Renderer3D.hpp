#pragma once

#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Light/Light.hpp"

namespace Pine::Renderer3D
{
    struct RenderConfiguration
    {
        // Global material override
        Material* OverrideMaterial = nullptr;

        // Global shader override
        Shader* OverrideShader = nullptr;

        // If the renderer should skip setting up materials during mesh preparation, OverrideShader
        // will still be accounted for though.
        bool SkipMaterialInitialization = false;
    };

    void Setup();
    void Shutdown();

    // Renderer specific global configuration
    RenderConfiguration& GetRenderConfiguration();

    // Resets the renderer for a new frame
    void FrameReset();

    // Prepares the specified mesh for rendering, overrideMaterial will override the mesh material if set.
    // If includeMaterial is set to false, the renderer won't set up the material for rendering.
    void PrepareMesh(Mesh* mesh, Material* overrideMaterial = nullptr);

    // Adds the transform to the ongoing instance batch, returns true if flushing is required, i.e. rendering via RenderMeshInstanced.
    bool AddInstance(const Matrix4f& transformationMatrix);

    // Renders the prepared mesh with a single transform
    void RenderMesh(const Matrix4f& transformationMatrix);

    // Renders the prepared mesh with the current instance batch, see Renderer3D::AddInstance(...)
    void RenderMeshInstanced();

    void AddLight(const Light* light);
    void UploadLights();

    void SetCamera(Camera* camera);
    Camera* GetCamera();

    void SetShader(Shader* shader);
}