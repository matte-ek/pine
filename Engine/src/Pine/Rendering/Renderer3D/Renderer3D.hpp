#pragma once

#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Light/Light.hpp"

#include <array>

namespace Pine
{
    struct RenderingContext;
}

namespace Pine::Renderer3D
{
    struct LightSlotData;

    struct RenderConfiguration
    {
        // Global material override
        Material* OverrideMaterial = nullptr;

        // Global shader override
        Shader* OverrideShader = nullptr;

        // If the renderer should skip setting up materials during mesh preparation, OverrideShader
        // will still be accounted for though.
        bool SkipMaterialInitialization = false;

        // If shader versions specified from the meshes should be ignored and just use default instead.
        bool IgnoreShaderVersions = false;
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

    // Prepares one chunk of a terrain, which blends several materials through a splat map rather
    // than drawing one.
    //
    // A sibling of PrepareMesh instead of an option on it: a terrain binds four of every texture
    // type and fills four of the material buffer's property slots, none of which the
    // single-material path has anywhere to put. It honours OverrideShader and
    // SkipMaterialInitialization exactly as PrepareMesh does, so the depth pre-pass and the shadow
    // passes draw terrain through their own shader without knowing it is terrain.
    //
    // A null layer draws as an untextured white surface, and 'splatTransform' is what maps the
    // chunk mesh's terrain-local uv onto the splat texture - see Terrain::GetSplatTransform.
    // 'brushRing' tints the ground under the editor's sculpting brush: xy its centre in
    // terrain-local units, z its radius, w the width of the band drawn at the rim. Null - which is
    // every caller outside the editor - draws the terrain through the plain shader, and the variant
    // that carries the overlay is never compiled.
    void PrepareTerrainChunk(Mesh* mesh,
                             const std::array<Material*, Specifications::TerrainLayers::COUNT>& layers,
                             Graphics::ITexture* splatMap,
                             const Vector4f& splatTransform,
                             const Vector4f* brushRing = nullptr);

    // Adds the transform to the ongoing instance batch, returns true if flushing is required, i.e. rendering via RenderMeshInstanced.
    //
    // 'lightSlots' is the lights that reach whatever is being drawn; a null one leaves the
    // instance's light indices at whatever the previous draw wrote, so anything that wants to be
    // lit has to pass its own.
    bool AddInstance(const Matrix4f& transformationMatrix, LightSlotData* lightSlots = nullptr);

    // Renders the prepared mesh with a single transform.
    //
    // 'indexCount' draws only the first that many indices, or the whole mesh when it is zero. That
    // exists for terrain: a chunk's skirt sits after its ground in the index buffer, so a pass that
    // must not let the skirt occlude anything simply draws fewer indices rather than needing a
    // second mesh without one.
    void RenderMesh(const Matrix4f& transformationMatrix,
                    LightSlotData* lightSlots = nullptr,
                    int writeStencilBuffer = 0x00,
                    std::uint32_t indexCount = 0);

    // Renders the prepared mesh with the current instance batch, see Renderer3D::AddInstance(...)
    void RenderMeshInstanced();

    void AddLight(Light* light);
    void UploadLights();

    void SetCamera(Camera* camera);
    void SetCamera(const Matrix4f &viewMatrix, const Matrix4f &projMatrix);

    // Renders from a single combined view-projection, as a ShadowView carries. The Matrices UBO
    // holds the two matrices separately and the shaders only ever use their product, so the view is
    // set to identity and the whole transform goes in the projection slot.
    void SetViewProjection(const Matrix4f& viewProjection);

    Camera* GetCamera();

    void SetShader(Shader* shader, ShaderVersion preferredVersion = 0);

    void PrepareScene(Vector3f ambientColor, Vector4f fogColor, float fogDistance, float fogIntensity);

    void UseRenderingContext(RenderingContext* renderingContext);
}
