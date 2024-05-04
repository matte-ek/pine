#include "OpenAL.hpp"

namespace Pine::Audio
{
    bool OpenAL::Setup()
    {
        // Get default device by passing nullptr
        m_Device = alcOpenDevice(nullptr);
        if (!m_Device)
            return false;

        m_Context = alcCreateContext(m_Device, nullptr);
        if (!m_Context)
            return false;

        if (!alcMakeContextCurrent(m_Context))
            return false;

        if (alcIsExtensionPresent(m_Device, "ALC_ENUMERATE_ALL_EXT"))
            m_DeviceName = alcGetString(m_Device, ALC_ALL_DEVICES_SPECIFIER);

        GetAudioDevices(alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER));

        Pine::Log::Verbose(fmt::format("Found default audio device: {}", m_DeviceName));

        return true;
    }

    void OpenAL::Shutdown()
    {
        alcDestroyContext(m_Context);
        alcCloseDevice(m_Device);
    }

    void OpenAL::GetAudioDevices(const ALCchar *devices)
    {
        while (*devices != '\0')
        {
            std::string deviceName(devices);

            if (std::find(m_DeviceList.begin(), m_DeviceList.end(), deviceName) == m_DeviceList.end())
            {
                m_DeviceList.emplace_back(deviceName);
            }

            devices += deviceName.size() + 1;
        }
    }
}