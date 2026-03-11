#pragma once
#include "Pine/Assets/Texture2D/Texture2D.hpp"

namespace nvtt
{
    struct Context;
}

namespace Pine::Importer
{

    class TextureImporter
    {
    private:
        static TextureImportData CompressImage(
            Texture2D* texture,
            const nvtt::Context* context,
            void* inputData,
            unsigned int width,
            unsigned int height,
            unsigned int channels);
    public:
        static bool Import(Texture2D* texture);
    };

}
