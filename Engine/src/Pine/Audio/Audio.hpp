#pragma once

#include "Pine/Audio/Interfaces/IAudioAPI.hpp"
#include "OpenAL/OpenAL.hpp"

namespace Pine::Audio
{
    // Initializes OpenAL
    bool Setup();

    // Frees OpenAL resources
    void Shutdown();
}
