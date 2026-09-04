#pragma once
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

        // Note: This is not guaranteed to be computed! Will only be done for
        // blend objects.
        float distance = 0.f;
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
        ObjectBatchMap OpaqueObjects;

        // Objects which will require discarding
        ObjectBatchMap DiscardObjects;

        // Objects which will require blending
        ObjectBatchMap BlendObjects;
    };
}

namespace Pine::Rendering::SceneProcessor
{
    struct SceneProcessorContext
    {
        std::unordered_map<RenderObject, std::uint32_t, RenderObjectHash> ModelInstanceCountHint;

        ObjectBatchData RenderingBatch;

        std::vector<Light*> Lights;

        // Renderers whose world bounds differ from last frame's.
        //
        // Kept as a list rather than a flag per object because its consumers ask "did anything move
        // inside *this* volume", and answering that has to stay O(movers) rather than O(scene) - a
        // cached shadow view that has to walk every object to learn it can skip its render has not
        // saved very much.
        std::vector<ModelRenderer*> MovedCasters;

        // Something was added, removed, disabled or had its model swapped this frame. Bounds cannot
        // describe an object that is no longer there, so this is the coarse signal that stands in
        // for it, and consumers should treat it as "assume everything moved".
        bool CasterSetChanged = false;

        // How many renderers were gathered, which is what CasterSetChanged is derived from.
        std::size_t CasterCount = 0;
    };

    void Prepare(SceneProcessorContext& context);
    void Run(SceneProcessorContext& context);

    // Clears the per-frame entity dirty flags.
    //
    // Split out of Prepare, where it sat behind a TODO saying it did not belong there, so that the
    // ordering is something a caller states rather than inherits: every consumer of IsDirty() has to
    // have run by the time this does. Prepare is no longer the last scene-level work in the frame -
    // the local shadow pass runs after it - so "the flags survive until rendering" stopped being
    // true of anything except by luck.
    //
    // Note that the shadow pass deliberately does *not* read these flags; see MovedCasters above
    // and Shadows::IsViewStale. They are not a signal a per-frame cache can be built on:
    // Transform::IsDirty() is cleared for every transform before Prepare in the editor but only for
    // objects that were actually drawn in production mode, and nothing clears a Light's at all.
    void EndFrame();
}
