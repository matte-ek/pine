#include "OpenAL.hpp"

#include <algorithm>

#include "Buffer/ALBuffer.hpp"
#include "Source/ALSource.hpp"
#include "Pine/Core/Log/Log.hpp"

namespace
{
    const char* AudioErrorToString(const ALenum error)
    {
        switch (error)
        {
            case AL_INVALID_NAME:
                return "AL_INVALID_NAME: a bad name (ID) was passed to an OpenAL function";
            case AL_INVALID_ENUM:
                return "AL_INVALID_ENUM: an invalid enum value was passed to an OpenAL function";
            case AL_INVALID_VALUE:
                return "AL_INVALID_VALUE: an invalid value was passed to an OpenAL function";
            case AL_INVALID_OPERATION:
                return "AL_INVALID_OPERATION: the requested operation is not valid";
            case AL_OUT_OF_MEMORY:
                return "AL_OUT_OF_MEMORY: the requested operation ran OpenAL out of memory";
            default:
                return "an unknown OpenAL error";
        }
    }
}

bool Pine::Audio::CheckAudioError(const char* operation)
{
    const auto error = alGetError();

    if (error == AL_NO_ERROR)
    {
        return false;
    }

    PError(fmt::format("{} failed with {}", operation, AudioErrorToString(error)));

    return true;
}

void Pine::Audio::ClearAudioError()
{
    alGetError();
}

bool Pine::Audio::OpenAL::Setup()
{
    // Passing nullptr asks for the system's default output device.
    m_Device = alcOpenDevice(nullptr);

    if (!m_Device)
    {
        PError("Failed to open the default audio device.");
        return false;
    }

    m_Context = alcCreateContext(m_Device, nullptr);

    if (!m_Context)
    {
        PError("Failed to create an audio context.");

        alcCloseDevice(m_Device);
        m_Device = nullptr;

        return false;
    }

    if (!alcMakeContextCurrent(m_Context))
    {
        PError("Failed to make the audio context current.");

        alcDestroyContext(m_Context);
        alcCloseDevice(m_Device);

        m_Context = nullptr;
        m_Device = nullptr;

        return false;
    }

    if (alcIsExtensionPresent(m_Device, "ALC_ENUMERATE_ALL_EXT"))
    {
        m_DeviceName = alcGetString(m_Device, ALC_ALL_DEVICES_SPECIFIER);

        ReadAudioDevices(alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER));
    }

    PVerbose(fmt::format("Found default audio device: {}", m_DeviceName));

    return true;
}

void Pine::Audio::OpenAL::Shutdown()
{
    if (m_Context != nullptr)
    {
        // Detach before destroying: destroying the current context is undefined, and the device
        // cannot be closed while a context still holds it.
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_Context);

        m_Context = nullptr;
    }

    if (m_Device != nullptr)
    {
        alcCloseDevice(m_Device);

        m_Device = nullptr;
    }
}

Pine::Audio::IAudioBuffer* Pine::Audio::OpenAL::CreateBuffer()
{
    return new ALBuffer();
}

void Pine::Audio::OpenAL::DestroyBuffer(IAudioBuffer* buffer)
{
    buffer->Dispose();

    delete dynamic_cast<ALBuffer*>(buffer);
}

Pine::Audio::IAudioSource* Pine::Audio::OpenAL::CreateSource()
{
    const auto source = new ALSource();

    if (!source->Setup())
    {
        delete source;

        return nullptr;
    }

    return source;
}

void Pine::Audio::OpenAL::DestroySource(IAudioSource* source)
{
    source->Dispose();

    delete dynamic_cast<ALSource*>(source);
}

void Pine::Audio::OpenAL::SetListenerTransform(
    const Vector3f& position,
    const Vector3f& forward,
    const Vector3f& up)
{
    ClearAudioError();

    alListener3f(AL_POSITION, position.x, position.y, position.z);

    // AL_ORIENTATION is a single six-float vector: the forward direction followed by the up one.
    const ALfloat orientation[6] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };

    alListenerfv(AL_ORIENTATION, orientation);

    CheckAudioError("Setting the listener transform");
}

void Pine::Audio::OpenAL::SetListenerVolume(const float volume)
{
    alListenerf(AL_GAIN, volume);
}

void Pine::Audio::OpenAL::ReadAudioDevices(const ALCchar* devices)
{
    if (devices == nullptr)
    {
        return;
    }

    // The list is a run of null-terminated names ended by a second null.
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

const std::string& Pine::Audio::OpenAL::GetDeviceName() const
{
    return m_DeviceName;
}

const std::vector<std::string>& Pine::Audio::OpenAL::GetDeviceList() const
{
    return m_DeviceList;
}
