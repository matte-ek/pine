#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Graphics/Interfaces/ITexture.hpp"
#include "stb_truetype.h"

namespace Pine
{

    struct FontData
    {
        float m_Size = 0.f;

        std::vector<stbtt_bakedchar> m_CharData;

        Graphics::ITexture* m_TextureFontAtlas = nullptr;
    };

    class Font : public IAsset
    {
    private:
        std::vector<FontData> m_FontAtlas;
    public:
        Font();

        std::uint32_t Create(float fontSize);

        const FontData& GetFontData(std::uint32_t index) const;

        bool LoadFromFile(AssetLoadStage stage) override;

        void Dispose() override;
    };

}