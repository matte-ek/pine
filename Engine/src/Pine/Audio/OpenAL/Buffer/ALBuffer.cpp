#include "ALBuffer.hpp"

#include <AL/al.h>

#include "Pine/Audio/OpenAL/OpenAL.hpp"
#include "Pine/Core/Log/Log.hpp"

void* Pine::Audio::ALBuffer::GetAudioIdentifier()
{
    return &m_Id;
}

std::uint32_t Pine::Audio::ALBuffer::GetId() const
{
    return m_Id;
}

bool Pine::Audio::ALBuffer::Upload(
    const void* data,
    const AudioFormat format,
    const int sampleRate,
    const int sampleCount)
{
    if (data == nullptr || sampleRate <= 0 || sampleCount <= 0)
    {
        PError("Refusing to upload an empty audio buffer.");
        return false;
    }

    ClearAudioError();

    if (m_Id == 0)
    {
        alGenBuffers(1, &m_Id);

        if (CheckAudioError("Generating an audio buffer"))
        {
            m_Id = 0;
            return false;
        }
    }

    const auto channels = AudioFormatChannelCount(format);
    const auto dataSize = static_cast<ALsizei>(sampleCount) * channels * static_cast<ALsizei>(sizeof(std::int16_t));

    alBufferData(
        m_Id,
        format == AudioFormat::Stereo16 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16,
        data,
        dataSize,
        sampleRate);

    if (CheckAudioError("Uploading audio buffer data"))
    {
        return false;
    }

    m_Format = format;
    m_SampleRate = sampleRate;
    m_SampleCount = sampleCount;

    return true;
}

void Pine::Audio::ALBuffer::Dispose()
{
    if (m_Id == 0)
    {
        return;
    }

    alDeleteBuffers(1, &m_Id);

    m_Id = 0;
    m_SampleCount = 0;
    m_SampleRate = 0;
}
