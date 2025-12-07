#pragma once

#include <algorithm>
#include "Pine/Audio/Interfaces/IAudioAPI.hpp"
#include "Pine/Core/Log/Log.hpp"
#include <AL/al.h>
#include <AL/alext.h>

namespace Pine::Audio
{
    class OpenAL : public IAudioAPI
    {
    private:
        std::string m_DeviceName;
        std::vector<std::string> m_DeviceList;

        ALCdevice* m_Device = nullptr;
        ALCcontext* m_Context = nullptr;
    public:
        bool Setup() override;
        void Shutdown() override;
        void GetAudioDevices(const ALCchar *devices);
    };
};