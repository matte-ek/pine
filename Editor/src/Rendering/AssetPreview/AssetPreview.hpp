#pragma once

#include "Pine/Core/Color/Color.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Pine
{
    class Asset;
}

namespace Pine::Graphics
{
    class IFrameBuffer;
}

namespace Editor::AssetPreview
{
    // Renders a single asset on its own, framed by a camera fitted to it, into a caller-supplied
    // frame buffer. The asset browser draws its icons and its large preview with this; the debug
    // server answers /asset/preview.png with it. It deliberately knows nothing about caching or
    // about what happens to the pixels afterwards - both callers want something different there.
    //
    // Every call must run on the main thread with the graphics context current. It leaves the
    // default frame buffer bound.

    // Looking straight down an axis flattens a silhouette; a three-quarter view reads the shape.
    // Yaw and pitch in degrees.
    constexpr Pine::Vector2f DefaultViewAngle = Pine::Vector2f(30.f, 20.f);

    struct Options
    {
        // The bottom-left region of the target that gets drawn, matching how a rendering context
        // uses a larger shared buffer. The target has to be at least this big.
        Pine::Vector2i Size = Pine::Vector2i(512);

        Pine::Vector2f ViewAngle = DefaultViewAngle;

        // Transparent by default, so an icon composites over whatever is behind it.
        Pine::Color Background = Pine::Color(0, 0, 0, 0);

        // The fill and the key light. Ambient is an authored sRGB colour, which PrepareScene
        // decodes, so values at or above 1.0 saturate the fill to white on their own.
        //
        // Both defaults are the asset browser's, where a bright silhouette reads well at 64x64.
        // They are well past clipping, so a preview meant to show an asset's shape wants far lower
        // ones - see how the debug server's preview route sets them. This pass writes the shader's
        // linear output into an LDR buffer with no display transform either (see Render), so
        // neither set is physically meaningful; both are tuned by eye.
        Pine::Vector3f Ambient = Pine::Vector3f(2.f);
        float LightIntensity = 2.f;
    };

    // Whether Render() has a subject to draw: a model renders as itself, a material on the editor's
    // preview sphere. No other asset type has a meaningful one.
    bool Supports(const Pine::Asset* asset);

    // Returns false without drawing when the asset has no subject, when its model holds no meshes,
    // or when the target cannot hold Size.
    bool Render(Pine::Asset* asset, const Options& options, Pine::Graphics::IFrameBuffer* target);
}
