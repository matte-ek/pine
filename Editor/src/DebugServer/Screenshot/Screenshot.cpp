#include "Screenshot.hpp"

#include <algorithm>
#include <cmath>
#include <string>

// Headers only: the Engine already compiles both implementations in TextureImporter.cpp, and
// defining them again here gives the linker duplicate symbols.
#include <stb/stb_image_write.h>
#include <stb/stb_image_resize2.h>

#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/RenderingContext.hpp"

namespace
{
    constexpr int m_Channels = 4;

    std::string EncodeBase64(const std::vector<std::uint8_t>& bytes)
    {
        constexpr const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        result.reserve((bytes.size() + 2) / 3 * 4);
        for (std::size_t index = 0; index < bytes.size(); index += 3)
        {
            const bool hasSecond = index + 1 < bytes.size();
            const bool hasThird = index + 2 < bytes.size();
            const std::uint32_t bits = (static_cast<std::uint32_t>(bytes[index]) << 16)
                | (hasSecond ? static_cast<std::uint32_t>(bytes[index + 1]) << 8 : 0)
                | (hasThird ? bytes[index + 2] : 0);
            result.push_back(alphabet[(bits >> 18) & 63]);
            result.push_back(alphabet[(bits >> 12) & 63]);
            result.push_back(hasSecond ? alphabet[(bits >> 6) & 63] : '=');
            result.push_back(hasThird ? alphabet[bits & 63] : '=');
        }
        return result;
    }

    // stb hands the encoder's output over in chunks rather than one buffer.
    void AppendEncodedBytes(void* userData, void* data, int size)
    {
        auto png = static_cast<std::vector<std::uint8_t>*>(userData);
        const auto bytes = static_cast<std::uint8_t*>(data);

        png->insert(png->end(), bytes, bytes + size);
    }
}

bool Editor::DebugServer::Screenshot::CapturePng(Pine::Graphics::IFrameBuffer* buffer,
                                                 const Pine::Vector2i region,
                                                 const int targetWidth,
                                                 std::vector<std::uint8_t>& png)
{
    if (buffer == nullptr)
    {
        return false;
    }

    const auto width = region.x;
    const auto height = region.y;

    if (width <= 0 || height <= 0 || width > buffer->GetSize().x || height > buffer->GetSize().y)
    {
        return false;
    }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * m_Channels);

    buffer->Bind();

    buffer->ReadPixels(Pine::Vector2i(0),
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

bool Editor::DebugServer::Screenshot::CapturePng(const Pine::RenderingContext* context,
                                                 const int targetWidth,
                                                 std::vector<std::uint8_t>& png)
{
    if (context == nullptr)
    {
        return false;
    }

    // The pipeline draws into the bottom-left Size region of a larger framebuffer - PostProcessing
    // sets the viewport to the context's size, not the attachment's - so only that region is read.
    // LevelViewportPanel samples exactly the same sub-rectangle when it displays the viewport.
    return CapturePng(context->FrameBuffer,
                      Pine::Vector2i(static_cast<int>(context->Size.x), static_cast<int>(context->Size.y)),
                      targetWidth,
                      png);
}

nlohmann::json Editor::DebugServer::Screenshot::DescribeImage(const std::vector<std::uint8_t>& png,
                                                              const int width,
                                                              const int height)
{
    return {
        { "contentType", "image/png" }, { "encoding", "base64" },
        { "width", width }, { "height", height }, { "data", EncodeBase64(png) }
    };
}
