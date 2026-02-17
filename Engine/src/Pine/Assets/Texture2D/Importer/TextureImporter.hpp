#pragma once
#include "Pine/Assets/Texture2D/Texture2D.hpp"

namespace Pine::Importer
{

    class TextureImporter
    {
    public:
        static bool Import(Texture2D* texture);
    };

}
