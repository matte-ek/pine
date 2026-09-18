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
        // Grouped by mesh and material, which is the order the scene batch already holds. Nothing
        // is sorted: the fewest draw calls, and no depth order at all.
        Batched,

        // Nearest surface first, which is what a depth pass wants - near geometry fills the depth
        // buffer before far geometry is rasterized, so the far geometry fails the depth test
        // before it is shaded.
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

        // How finely an ordered mode resolves distance.
        //
        // 0 sorts exactly: every item lands in depth order, and batching is left to whichever
        // neighbours happen to share a mesh and a material. On a level built from a modular kit
        // that is close to none of them - see docs/rendering.md for what it measured.
        //
        // A positive count spreads the items across that many buckets between the nearest and the
        // furthest of them and orders by bucket, so items at a similar depth stay grouped by mesh
        // and material and still submit as one draw. Fewer buckets, fewer draw calls, coarser
        // depth order. Blending needs exact order and so has to pass 0.
        int DepthBuckets = 0;
    };

    // The draw work for ONE view, flattened out of the scene batch and put in an order.
    //
    // Order is a property of (work, viewer) the same way visibility is, so a list is built for one
    // view and consumed by it: two viewports, or a shadow view and the scene camera, each build
    // their own. The scene batch stays viewer-independent.
    //
    // What survives the sort is runs of consecutive items sharing a mesh and a material, and a run
    // is what the submitter turns back into a single instanced draw. Batching is therefore derived
    // from the order rather than imposed on it, which is what lets one mechanism serve a pass that
    // needs depth order and a pass that only wants the fewest draw calls.
    class DrawList
    {
        std::vector<DrawItem> m_Items;
    public:
        // Collects everything in 'batch' that is in 'mode' and passes 'visibility', then puts it
        // in the order asked for.
        //
        // Storage is kept between calls, so rebuilding a list every frame stops allocating once it
        // has grown.
        void Build(const ObjectBatchMap& batch,
                   MaterialRenderingMode mode,
                   const RenderCulling::VisibilitySet& visibility,
                   const DrawOrdering& ordering);

        const std::vector<DrawItem>& GetItems() const;
    };
}
