#pragma once

#include <cstdint>
#include <vector>

namespace Pine
{
    struct RenderingContext;
}

namespace Editor::DebugServer::Screenshot
{
    // Reads back what a rendering context drew this frame and encodes it as a PNG.
    //
    // targetWidth scales the result down, keeping the aspect ratio; 0 or wider than the source keeps
    // the native size. Must run on the main thread with the GL context current, and after the frame
    // has been rendered.
    //
    // Returns false, leaving png untouched, when the context has nothing to read: no framebuffer, or
    // a zero-sized viewport because its panel is not visible.
    bool CapturePng(const Pine::RenderingContext* context, int targetWidth, std::vector<std::uint8_t>& png);
}
