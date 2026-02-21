#include "Font.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include <fstream>

#define STB_TRUETYPE_IMPLEMENTATION

#include <stb_truetype.h>

Pine::Font::Font()
{
    m_Type = AssetType::Font;
}

std::uint32_t Pine::Font::Create(float fontSize)
{
    std::ifstream stream(m_FilePath.string(), std::ios::binary | std::ios::ate);

    if (!stream.is_open())
    {
        return false;
    }

    std::streamsize size = stream.tellg();

    stream.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);

    if (stream.read(buffer.data(), size))
    {
        void* bitmapBuffer = malloc(1024 * 1024);

        FontData data;

        data.m_Size = fontSize;
        data.m_CharData.resize(96);

        /*
         * Baked fonts can't have subpixel positioning
         */
        // stbtt_BakeFontBitmap(reinterpret_cast<const unsigned char*>(buffer.data()), 0, fontSize, static_cast<unsigned char*>(bitmapBuffer), 1024, 1024, 32, 96, data.m_CharData.data());

        // For several font sizes we use a range array and set the font sizes we want in it then pack it with stbtt_PackFontRanges
        stbtt_pack_context ctx;
        stbtt_PackBegin(&ctx, static_cast<unsigned char*>(bitmapBuffer), 1024, 1024, 0, 1, NULL);
        stbtt_PackSetOversampling(&ctx, 1, 1);
        stbtt_pack_range range[1] = {{fontSize, 32, nullptr, 96, data.m_CharData.data(), 0, 0}};

        stbtt_PackFontRanges(&ctx, reinterpret_cast<const unsigned char*>(buffer.data()), 0, range, 1);
        stbtt_PackEnd(&ctx);

        data.m_TextureFontAtlas = Graphics::GetGraphicsAPI()->CreateTexture();

        data.m_TextureFontAtlas->Bind();
        data.m_TextureFontAtlas->UploadTextureData(1024, 1024, Graphics::TextureFormat::SingleChannel, Graphics::TextureDataFormat::UnsignedByte, bitmapBuffer);
        data.m_TextureFontAtlas->SetSwizzleMask(Graphics::SwizzleMaskChannel::Red, Graphics::SwizzleMaskChannel::Red, Graphics::SwizzleMaskChannel::Red, Graphics::SwizzleMaskChannel::Alpha);

        free(bitmapBuffer);

        m_FontAtlas.push_back(data);

        return static_cast<std::uint32_t>(m_FontAtlas.size()) - 1;
    }

    return 0;
}

const Pine::FontData& Pine::Font::GetFontData(std::uint32_t index) const
{
    return m_FontAtlas[index];
}

void Pine::Font::Dispose()
{
}