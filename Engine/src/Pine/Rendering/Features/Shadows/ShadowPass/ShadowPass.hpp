#pragma once

namespace Pine::Rendering
{
    struct ObjectBatchData;
    struct ShadowView;
}

namespace Pine::Rendering::ShadowPass
{
    void Setup();

    // Renders a run of shadow views into the shadow atlas, cascades and local lights alike. The
    // view carries everything that differs between them: its tile, face culling and bias pair.
    //
    // Views whose NeedsRender is false are skipped. RecordRenderedViews in Shadows.cpp walks the
    // same run on the same condition afterwards, so the two must stay in agreement.
    void Render(const ShadowView* views, int count, const ObjectBatchData& batchData);
}
