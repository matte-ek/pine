#pragma once

namespace Pine::Audio
{
    // The sample layout of a block of PCM. Both of these are signed 16-bit samples, interleaved
    // when there is more than one channel - that is the one representation the decoders produce
    // and the only one an AudioFile uploads.
    //
    // Serialized as an integer by AudioFile, so only ever append.
    enum class AudioFormat
    {
        Mono16 = 0,
        Stereo16
    };

    inline const char* AudioFormatToString(const AudioFormat format)
    {
        switch (format)
        {
            case AudioFormat::Mono16:
                return "Mono, 16-bit";
            case AudioFormat::Stereo16:
                return "Stereo, 16-bit";
            default:
                return "Unknown";
        }
    }

    inline int AudioFormatChannelCount(const AudioFormat format)
    {
        return format == AudioFormat::Stereo16 ? 2 : 1;
    }

    // A block of decoded PCM held by the audio device, ready to be played. Owned by the AudioFile
    // asset it was decoded from and shared by every source playing that asset, so nothing here is
    // about playback - a buffer has no position, volume or play state of its own.
    class IAudioBuffer
    {
    protected:
        AudioFormat m_Format = AudioFormat::Mono16;

        int m_SampleRate = 0;

        // Counted per channel, so a mono and a stereo clip of the same length report the same
        // number. Multiply by the channel count for the number of samples actually stored.
        int m_SampleCount = 0;
    public:
        virtual ~IAudioBuffer() = default;

        // The type is up to the audio API being used.
        virtual void* GetAudioIdentifier() = 0;

        // Replaces whatever the buffer held with 'sampleCount' * channels interleaved 16-bit
        // samples read from 'data'. The audio device takes its own copy, so the caller keeps
        // ownership of 'data' and is free to release it immediately afterwards.
        virtual bool Upload(const void* data, AudioFormat format, int sampleRate, int sampleCount) = 0;

        virtual void Dispose() = 0;

        AudioFormat GetFormat() const
        {
            return m_Format;
        }

        int GetSampleRate() const
        {
            return m_SampleRate;
        }

        int GetSampleCount() const
        {
            return m_SampleCount;
        }

        float GetDuration() const
        {
            if (m_SampleRate <= 0)
            {
                return 0.f;
            }

            return static_cast<float>(m_SampleCount) / static_cast<float>(m_SampleRate);
        }
    };
}
