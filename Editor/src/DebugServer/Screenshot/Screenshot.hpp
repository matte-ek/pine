#pragma once

#include <cstdint>
#include <vector>

#include <nlohmann/json.hpp>

#include "Pine/Core/Math/Math.hpp"

namespace Pine
{
    struct RenderingContext;
}

namespace Pine::Graphics
{
    class IFrameBuffer;
}

namespace Editor::DebugServer::Screenshot
{
    // Reads pixels back off a frame buffer and encodes them as a PNG.
    //
    // targetWidth scales the result down, keeping the aspect ratio; 0 or wider than the source keeps
    // the native size. Must run on the main thread with the GL context current, and after whatever
    // drew into the buffer has finished.

    // Reads the bottom-left `region` of the buffer, which is the part a rendering context or an
    // asset preview draws into when the attachment behind it is larger.
    //
    // Returns false, leaving png untouched, when there is nothing to read: no buffer, or a region
    // that is empty or larger than the attachment.
    bool CapturePng(Pine::Graphics::IFrameBuffer* buffer, Pine::Vector2i region, int targetWidth,
                    std::vector<std::uint8_t>& png);

    // The same for what a rendering context drew this frame. Returns false when the context has no
    // frame buffer, or a zero-sized viewport because its panel is not visible.
    bool CapturePng(const Pine::RenderingContext* context, int targetWidth, std::vector<std::uint8_t>& png);

    // The "image" object every route that answers with a rendered PNG returns, so a client can read
    // all of them the same way. JSON carries no binary, hence base64.
    nlohmann::json DescribeImage(const std::vector<std::uint8_t>& png, int width, int height);
}
