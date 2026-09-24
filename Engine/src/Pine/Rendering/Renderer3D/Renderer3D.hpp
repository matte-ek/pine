#pragma once

#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Graphics/Interfaces/IStorageBuffer.hpp"
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

    // Which material a mesh is really drawn with: the global override if one is set, otherwise the
    // caller's override, otherwise the mesh's own. Null when none of the three has one.
    //
    // Exposed because the submitter has to answer the same question PrepareMesh does, before it
    // calls it: a material decides face culling, and culling is pipeline state that has to be set
    // around the draw rather than during mesh preparation. Sharing the rule keeps the two from
    // disagreeing about which material a draw belongs to.
    Material* ResolveMaterial(Mesh* mesh, Material* overrideMaterial = nullptr);

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

    // Prepares a mesh of a terrain detail model, which RenderTerrainDetail then draws many copies
    // of. Returns false when there is nothing that can draw it - no material, or no program for
    // the detail version - in which case the caller must skip the draw: any other program would
    // read the placements out of the Instances block and put every copy in the wrong place.
    //
    // Always draws through the engine's generic shader, whatever shader the mesh's material names,
    // because only that shader has the detail version. The material still supplies the textures,
    // colours and rendering mode. Detail is drawn in the scene pass only, where there is no blend
    // pass around it, so a Transparent material is drawn as a Discard one.
    bool PrepareTerrainDetailMesh(Mesh* mesh);

    // Draws the prepared detail mesh once per placement in 'instances', which holds
    // 'instanceCount' ShaderStorages::TerrainDetailInstanceData entries. The placements are
    // terrain-local; 'terrainTransform' puts the terrain in the world, and every copy is lit
    // through 'lightSlots'.
    //
    // 'fadeDistances' are the distances from the camera at which a copy starts shrinking and at
    // which it has shrunk to nothing, so detail sinks into the ground at the edge of its draw
    // distance rather than popping out of existence.
    void RenderTerrainDetail(const Matrix4f& terrainTransform,
                             LightSlotData* lightSlots,
                             Graphics::IStorageBuffer* instances,
                             int instanceCount,
                             const Vector2f& fadeDistances);

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

    // The wind the shaders sway foliage with. The default is still air.
    struct SceneWind
    {
        // The direction the wind blows along the ground, as a unit (x, z).
        Vector2f Direction = Vector2f(1.f, 0.f);

        // How far a tip leans at most, as a share of its height above the ground.
        float Strength = 0.f;

        // Where the gusts are in their cycle, in radians.
        float Phase = 0.f;
    };

    void PrepareScene(Vector3f ambientColor, Vector4f fogColor, float fogDistance, float fogIntensity, const SceneWind& wind = {});

    void UseRenderingContext(RenderingContext* renderingContext);
}
