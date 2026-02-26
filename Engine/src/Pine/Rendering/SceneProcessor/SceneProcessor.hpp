#pragma once
#include <unordered_map>

#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"

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
    };

    void Prepare(SceneProcessorContext& context);
    void Run(SceneProcessorContext& context);
}
