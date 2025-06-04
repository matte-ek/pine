#pragma once

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Rendering/RenderingContext.hpp"

namespace Pine
{
    class Model;
    class ModelRenderer;
    class Light;
}

namespace Pine::Pipeline3D
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
        Model* Model = nullptr;
        Material* OverrideMaterial = nullptr;

        bool operator==(const RenderObject& other) const
        {
            return Model == other.Model && OverrideMaterial == other.OverrideMaterial;
        }
    };

    struct RenderObjectHash
    {
        size_t operator()(const RenderObject& key) const
        {
            const std::size_t modelHash = std::hash<Model*>()(key.Model);
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

    struct PipelineConfiguration
    {
        bool RenderShadows = true;
        bool RenderSkybox = true;
    };

    void Setup();
    void Shutdown();

    void Run(RenderingContext& context);

    PipelineConfiguration& GetPipelineConfiguration();
}
