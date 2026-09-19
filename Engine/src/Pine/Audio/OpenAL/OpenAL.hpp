#pragma once

#include <string>
#include <vector>

#include <AL/al.h>
#include <AL/alc.h>

#include "Pine/Audio/Interfaces/IAudioAPI.hpp"

namespace Pine::Audio
{
    // Drains OpenAL's error flag and reports whether it held anything, logging what 'operation'
    // was when it did. OpenAL keeps a single sticky error per context rather than returning a
    // status, so a call that is never checked leaves its error behind for whatever checks next -
    // which is why every al* call that can fail is followed by one of these.
    bool CheckAudioError(const char* operation);

    // Throws away any error left over from earlier, un-checked calls, so that the next
    // CheckAudioError() reports on the call in between and not on something unrelated.
    void ClearAudioError();

    class OpenAL final : public IAudioAPI
    {
    private:
        std::string m_DeviceName;
        std::vector<std::string> m_DeviceList;

        ALCdevice* m_Device = nullptr;
        ALCcontext* m_Context = nullptr;

        // Reads OpenAL's double-null-terminated device name list into m_DeviceList.
        void ReadAudioDevices(const ALCchar* devices);
    public:
        bool Setup() override;
        void Shutdown() override;

        IAudioBuffer* CreateBuffer() override;
        void DestroyBuffer(IAudioBuffer* buffer) override;

        IAudioSource* CreateSource() override;
        void DestroySource(IAudioSource* source) override;

        void SetListenerTransform(const Vector3f& position, const Vector3f& forward, const Vector3f& up) override;
        void SetListenerVolume(float volume) override;

        const std::string& GetDeviceName() const;
        const std::vector<std::string>& GetDeviceList() const;
    };
}
