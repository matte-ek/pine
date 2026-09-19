#pragma once

#include "Pine/Assets/AudioFile/AudioFile.hpp"

namespace Pine::Importer
{

    class AudioImporter
    {
    public:
        static bool Import(AudioFile* audioFile);
    };

}
