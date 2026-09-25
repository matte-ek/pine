#pragma once
#include <optional>
#include <unordered_map>

#include "Pine/Assets/Material/Material.hpp"

namespace Pine
{
    class Light;
    class Model;
    class ModelRenderer;
}

namespace Pine::Rendering
{
    struct ObjectRenderInstance
    {
        ModelRenderer* renderer = nullptr;
    };

    struct RenderObject
    {
        Model* ModelPtr = nullptr;
        Material* OverrideMaterial = nullptr;

        bool operator==(const RenderObject& other) const
        {
            return ModelPtr == other.ModelPtr && OverrideMaterial == other.OverrideMaterial;
        }
    };

    struct RenderObjectHash
    {
        size_t operator()(const RenderObject& key) const
        {
            const std::size_t modelHash = std::hash<Model*>()(key.ModelPtr);
            const std::size_t materialHash = std::hash<Material*>()(key.OverrideMaterial);

            return modelHash ^ (materialHash << 1);
        }
    };

    typedef std::unordered_map<RenderObject, std::vector<ObjectRenderInstance>, RenderObjectHash> ObjectBatchMap;

    struct ObjectBatchData
    {
        // Everything in the scene, whatever its material's rendering mode - a draw list filters it
        // down to the mode its pass wants.
        ObjectBatchMap OpaqueObjects;

        // Only the objects carrying a mesh with a Transparent material, so the blend pass does not
        // have to filter the whole scene before it sorts.
        ObjectBatchMap BlendObjects;
    };
}

namespace Pine::Rendering::SceneProcessor
{
    struct SceneProcessorContext
    {
        // Where LOD distances are measured from this frame, written by Pipeline3D::Prepare before
        // Prepare runs. Unset when no context has a camera, and every renderer then draws the model
        // it names.
        std::optional<Vector3f> LodReferencePosition;

        std::unordered_map<RenderObject, std::uint32_t, RenderObjectHash> ModelInstanceCountHint;

        ObjectBatchData RenderingBatch;

        std::vector<Light*> Lights;

        // Shadow casters whose world bounds or LOD level differ from last frame's, plus renderers
        // that started or stopped casting. A list rather than a per-object flag, so a consumer
        // asking "did anything move inside this volume" walks only the movers.
        std::vector<ModelRenderer*> MovedCasters;

        // Something was added, removed, disabled or had its model swapped this frame. Consumers
        // should treat it as "assume everything moved".
        bool CasterSetChanged = false;

        // How many shadow-casting renderers were gathered, which is what CasterSetChanged is derived
        // from. A renderer that casts nothing is left out, so adding or removing one invalidates no
        // shadow view. Counts the ones hidden by distance too: those come and go through
        // MovedCasters instead, which invalidates only the shadow views they are in.
        std::size_t CasterCount = 0;

        // A light moved, changed type, or was created or destroyed this frame, so every cached
        // light slot in the scene is suspect. Lights::Prepare acts on it for the model renderers
        // itself; terrain reads it for its chunks.
        bool LightSetChanged = false;

        // A terrain was moved, reshaped, added or removed this frame. Written by
        // TerrainRenderer::Prepare, since the scene processor does not walk terrain.
        bool TerrainChanged = false;
    };

    void Prepare(SceneProcessorContext& context);
    void Run(SceneProcessorContext& context);

    // Clears the per-frame entity dirty flags. Call once every consumer of Entity::IsDirty() has
    // run for the frame, which includes the local shadow pass after Prepare.
    //
    // The dirty flags are not reliable enough for a cache: Transform's are cleared differently in
    // the editor and in production, and nothing clears a Light's. The shadow tile cache uses
    // MovedCasters instead.
    void EndFrame();
}
