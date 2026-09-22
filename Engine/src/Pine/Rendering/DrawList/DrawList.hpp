#pragma once

#include <vector>

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"

namespace Pine
{
    class Mesh;
    class ModelRenderer;
}

namespace Pine::Rendering
{
    // One mesh of one object, in the order it will be submitted.
    struct DrawItem
    {
        Mesh* MeshPtr = nullptr;

        // What the mesh is drawn with: the object's override material when it has one, otherwise
        // the mesh's own. Null means the mesh carries no material, which the batch treats as
        // "belongs to whichever mode is asking" - see Build.
        Material* MaterialPtr = nullptr;

        ModelRenderer* Renderer = nullptr;

        // To the view this list was built for, in whichever metric its order asked for. Left at 0
        // for an order that does not need it.
        float Distance = 0.f;
    };

    enum class DrawOrder
    {
        // Grouped by mesh and material, as the scene batch already is. No sort, fewest draw calls.
        Batched,

        // Nearest surface first, so far geometry fails the depth test before it is shaded.
        FrontToBack,

        // Furthest centre first, the order alpha blending has to be submitted in.
        BackToFront
    };

    // How a list is ordered, and what the order is measured against.
    struct DrawOrdering
    {
        DrawOrder Order = DrawOrder::Batched;

        // Where the view is. Read by the ordered modes only.
        Vector3f ViewPosition = Vector3f(0.f);

        // How finely an ordered mode resolves distance. 0 sorts exactly, which breaks up most
        // batching. A positive count orders by that many depth buckets instead, keeping items
        // within a bucket grouped by mesh and material. Blending needs exact order, so it passes 0.
        // See docs/rendering.md for measurements.
        int DepthBuckets = 0;
    };

    // The draw work for one view, flattened out of the scene batch and put in an order. Each view
    // builds its own; the scene batch stays viewer-independent.
    //
    // Pipeline3D::RenderBatch turns each run of consecutive items sharing a mesh and a material
    // into one instanced draw, so batching follows from the order.
    class DrawList
    {
        std::vector<DrawItem> m_Items;
    public:
        // Collects everything in 'batch' that is in 'mode' and passes 'visibility', then puts it
        // in the order asked for.
        //
        // Storage is kept between calls.
        void Build(const ObjectBatchMap& batch,
                   MaterialRenderingMode mode,
                   const RenderCulling::VisibilitySet& visibility,
                   const DrawOrdering& ordering);

        const std::vector<DrawItem>& GetItems() const;
    };
}
