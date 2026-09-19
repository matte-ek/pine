#include "AudioFile.hpp"

#include "Importer/AudioImporter.hpp"
#include "Pine/Audio/Audio.hpp"
#include "Pine/Threading/Threading.hpp"

Pine::AudioFile::AudioFile()
{
    m_Type = AssetType::Audio;
}

bool Pine::AudioFile::LoadAssetData(const ByteSpan& span)
{
    AudioSerializer audioSerializer;

    if (!audioSerializer.Read(span))
    {
        return false;
    }

    audioSerializer.Format.Read(m_Format);
    audioSerializer.SampleRate.Read(m_SampleRate);
    audioSerializer.SampleCount.Read(m_SampleCount);
    audioSerializer.ImportForceMono.Read(m_ImportConfiguration.ForceMono);

    std::vector<std::int16_t> samples;

    audioSerializer.Samples.Read(samples);

    if (samples.empty())
    {
        PWarning(fmt::format("Audio asset '{}' holds no samples.", m_Path));
        return false;
    }

    // The engine runs without an output device, so the clip is loaded either way - it is a
    // perfectly good asset that simply has nowhere to play from, and failing here would take down
    // every level that references a sound. Audio::Setup() has already said so once; saying it
    // again per clip would bury the rest of the log.
    if (!Audio::HasInitializedAudioAPI())
    {
        m_State = AssetState::Loaded;

        return true;
    }

    // Assets load on a worker thread, so hand the upload to the main thread the way a texture
    // does. An OpenAL context belongs to whichever thread made it current, and uploading from
    // somewhere else is the kind of mistake that works right up until it doesn't.
    auto uploaded = false;

    const auto uploadTask = Threading::QueueTask<void>([this, &samples, &uploaded]()
    {
        if (m_Buffer == nullptr)
        {
            m_Buffer = Audio::GetAudioAPI()->CreateBuffer();
        }

        uploaded = m_Buffer->Upload(samples.data(), m_Format, m_SampleRate, m_SampleCount);
    },
    TaskThreadingMode::MainThread);

    Threading::AwaitTaskResult(uploadTask);

    if (!uploaded)
    {
        PError(fmt::format("Failed to upload audio asset '{}' to the audio device.", m_Path));
        return false;
    }

    m_State = AssetState::Loaded;

    return true;
}

Pine::ByteSpan Pine::AudioFile::SaveAssetData()
{
    AudioSerializer audioSerializer;

    // The decoded samples only exist here right after an import. Every other save - the user
    // changing an import setting, say - has to carry the stored PCM across itself, by reading back
    // the '.passet' being replaced. Keeping a clip in system memory purely so that it could be
    // written out again would double what audio costs, for something that happens by hand and
    // rarely.
    if (m_ImportSamples.empty())
    {
        audioSerializer.Read(ReadStoredAssetData());
    }
    else
    {
        audioSerializer.Samples.Write(m_ImportSamples);

        m_ImportSamples.clear();
        m_ImportSamples.shrink_to_fit();
    }

    audioSerializer.Format.Write(m_Format);
    audioSerializer.SampleRate.Write(m_SampleRate);
    audioSerializer.SampleCount.Write(m_SampleCount);
    audioSerializer.ImportForceMono.Write(m_ImportConfiguration.ForceMono);

    return audioSerializer.Write();
}

bool Pine::AudioFile::Import(Importer::AssetImport* context)
{
    return Importer::AudioImporter::Import(this);
}

void Pine::AudioFile::Dispose()
{
    if (m_Buffer != nullptr)
    {
        if (Audio::HasInitializedAudioAPI())
        {
            Audio::GetAudioAPI()->DestroyBuffer(m_Buffer);
        }

        m_Buffer = nullptr;
    }

    m_ImportSamples.clear();
    m_ImportSamples.shrink_to_fit();

    m_State = AssetState::Unloaded;
}

Pine::Audio::AudioFormat Pine::AudioFile::GetFormat() const
{
    return m_Format;
}

int Pine::AudioFile::GetChannelCount() const
{
    return Audio::AudioFormatChannelCount(m_Format);
}

int Pine::AudioFile::GetSampleRate() const
{
    return m_SampleRate;
}

int Pine::AudioFile::GetSampleCount() const
{
    return m_SampleCount;
}

float Pine::AudioFile::GetDuration() const
{
    if (m_SampleRate <= 0)
    {
        return 0.f;
    }

    return static_cast<float>(m_SampleCount) / static_cast<float>(m_SampleRate);
}

std::size_t Pine::AudioFile::GetSampleDataSize() const
{
    return static_cast<std::size_t>(m_SampleCount) * GetChannelCount() * sizeof(std::int16_t);
}

Pine::AudioImportConfiguration& Pine::AudioFile::GetImportConfiguration()
{
    return m_ImportConfiguration;
}

Pine::Audio::IAudioBuffer* Pine::AudioFile::GetBuffer() const
{
    return m_Buffer;
}
