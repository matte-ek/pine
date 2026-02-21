#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Graphics/Interfaces/ITexture.hpp"
#include <stb_truetype.h>

namespace Pine
{

    struct FontData
    {
        float m_Size = 0.f;

        std::vector<stbtt_packedchar> m_CharData;

        Graphics::ITexture* m_TextureFontAtlas = nullptr;
    };

    class Font : public Asset
    {
    private:
        std::vector<FontData> m_FontAtlas;
    public:
        Font();

        std::uint32_t Create(float fontSize);

        const FontData& GetFontData(std::uint32_t index) const;

        void Dispose() override;
    };

}