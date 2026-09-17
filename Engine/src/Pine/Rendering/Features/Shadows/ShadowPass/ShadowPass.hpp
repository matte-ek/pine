#pragma once

namespace Pine::Rendering
{
    struct ObjectBatchData;
    struct ShadowView;
}

namespace Pine::Rendering::ShadowPass
{
    void Setup();

    // Renders a run of shadow views into the shadow atlas.
    //
    // The one place shadow depth is drawn, for cascades and local lights alike. Everything that used
    // to differ between the two is carried by the view now - its tile, its face culling, its bias
    // pair - so this loop has no idea which kind it is looking at. That deletion is what folding the
    // cascades into the atlas actually bought; the memory saving was a side effect.
    //
    // Views whose NeedsRender is false are skipped, and skipping is the whole point of the flag:
    // whoever owns the cache bookkeeping behind it has to walk the same run on the same condition
    // afterwards. See RecordRenderedViews in Shadows.cpp, which is the other half of this.
    void Render(const ShadowView* views, int count, const ObjectBatchData& batchData);
}
