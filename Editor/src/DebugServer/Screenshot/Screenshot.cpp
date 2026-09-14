#include "Screenshot.hpp"

#include <algorithm>
#include <cmath>

// Headers only: the Engine already compiles both implementations in TextureImporter.cpp, and
// defining them again here gives the linker duplicate symbols.
#include <stb/stb_image_write.h>
#include <stb/stb_image_resize2.h>

#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/RenderingContext.hpp"

namespace
{
    constexpr int m_Channels = 4;

    // stb hands the encoder's output over in chunks rather than one buffer.
    void AppendEncodedBytes(void* userData, void* data, int size)
    {
        auto png = static_cast<std::vector<std::uint8_t>*>(userData);
        const auto bytes = static_cast<std::uint8_t*>(data);

        png->insert(png->end(), bytes, bytes + size);
    }
}

bool Editor::DebugServer::Screenshot::CapturePng(const Pine::RenderingContext* context,
                                                 const int targetWidth,
                                                 std::vector<std::uint8_t>& png)
{
    if (context == nullptr || context->FrameBuffer == nullptr)
    {
        return false;
    }

    // The pipeline draws into the bottom-left Size region of a larger framebuffer - PostProcessing
    // sets the viewport to the context's size, not the attachment's - so only that region is read.
    // LevelViewportPanel samples exactly the same sub-rectangle when it displays the viewport.
    const auto width = static_cast<int>(context->Size.x);
    const auto height = static_cast<int>(context->Size.y);

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * m_Channels);

    context->FrameBuffer->Bind();

    context->FrameBuffer->ReadPixels(Pine::Vector2i(0),
                                     Pine::Vector2i(width, height),
                                     Pine::Graphics::ReadFormat::RGBA,
                                     Pine::Graphics::TextureDataFormat::UnsignedByte,
                                     pixels.size(),
                                     pixels.data());

    // By the time the debug server drains its queue, ImGui has already drawn this frame to the
    // default framebuffer. Put the binding back the way we found it.
    Pine::Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr);

    int outputWidth = width;
    int outputHeight = height;

    if (targetWidth > 0 && targetWidth < width)
    {
        outputWidth = targetWidth;
        outputHeight = std::max(1, static_cast<int>(std::lround(
            static_cast<double>(height) * targetWidth / width)));

        std::vector<std::uint8_t> scaled(static_cast<std::size_t>(outputWidth) * outputHeight * m_Channels);

        // sRGB-aware: averaging encoded values directly would darken the result.
        stbir_resize_uint8_srgb(pixels.data(), width, height, 0,
                                scaled.data(), outputWidth, outputHeight, 0,
                                STBIR_RGBA);

        pixels = std::move(scaled);
    }

    // OpenGL reads rows bottom-up, PNG stores them top-down. Safe as a global because captures only
    // ever happen on the main thread.
    stbi_flip_vertically_on_write(1);

    png.clear();

    const int written = stbi_write_png_to_func(AppendEncodedBytes,
                                               &png,
                                               outputWidth,
                                               outputHeight,
                                               m_Channels,
                                               pixels.data(),
                                               outputWidth * m_Channels);

    stbi_flip_vertically_on_write(0);

    return written != 0 && !png.empty();
}
