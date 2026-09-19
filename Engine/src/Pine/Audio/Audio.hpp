#pragma once

#include "Pine/Audio/Interfaces/IAudioAPI.hpp"

namespace Pine::Audio
{
    // Creates and initializes the audio API, returns false if there is no usable output device.
    bool Setup();

    // Call before application exit, to free the audio device and everything it holds.
    void Shutdown();

    bool HasInitializedAudioAPI();
    IAudioAPI* GetAudioAPI();
}
