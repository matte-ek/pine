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
            const void* inputData,
            unsigned int width,
            unsigned int height,
            unsigned int channels,
            bool hasGpuAcceleration);
    public:
        static bool Import(Texture2D* texture);
    };

}
