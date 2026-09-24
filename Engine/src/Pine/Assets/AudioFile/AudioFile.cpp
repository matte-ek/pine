#include "AudioFile.hpp"

#include "Importer/AudioImporter.hpp"
#include "Importer/AudioLoader/AudioLoader.hpp"
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

    m_Encoding = AudioEncoding::Pcm;

    audioSerializer.Encoding.Read(m_Encoding);
    audioSerializer.Format.Read(m_Format);
    audioSerializer.SampleRate.Read(m_SampleRate);
    audioSerializer.SampleCount.Read(m_SampleCount);
    audioSerializer.ImportForceMono.Read(m_ImportConfiguration.ForceMono);

    std::vector<std::int16_t> samples;
    std::vector<std::uint8_t> encodedData;

    switch (m_Encoding)
    {
        case AudioEncoding::Pcm:
            audioSerializer.Samples.Read(samples);
            break;
        case AudioEncoding::Vorbis:
            audioSerializer.EncodedData.Read(encodedData);
            break;
        default:
            PError(fmt::format("Audio asset '{}' is stored in an encoding this engine does not know.", m_Path));
            return false;
    }

    if (samples.empty() && encodedData.empty())
    {
        PWarning(fmt::format("Audio asset '{}' holds no samples.", m_Path));
        return false;
    }

    // The engine runs without an output device, so the clip is loaded either way - it is a
    // perfectly good asset that simply has nowhere to play from, and failing here would take down
    // every level that references a sound. Audio::Setup() has already said so once; saying it
    // again per clip would bury the rest of the log. A Vorbis clip is not decoded at all, since
    // there is nothing to hand the samples to.
    if (!Audio::HasInitializedAudioAPI())
    {
        m_State = AssetState::Loaded;

        return true;
    }

    if (m_Encoding == AudioEncoding::Vorbis && !DecodeVorbis(encodedData, samples))
    {
        return false;
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
        else
        {
            // A re-import uploads into the buffer already there, and OpenAL refuses while a voice has it bound.
            Audio::Internal::StopPreviewOf(*this);
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

    // The clip's samples or Vorbis bytes only exist here right after an import. Every other save -
    // the user changing an import setting, say - has to carry the stored ones across itself, by
    // reading back the '.passet' being replaced. Keeping a clip in system memory purely so that it
    // could be written out again would double what audio costs, for something that happens by
    // hand and rarely.
    const auto hasImportData = !m_ImportSamples.empty() || !m_ImportEncodedData.empty();

    if (!hasImportData)
    {
        audioSerializer.Read(ReadStoredAssetData());
    }
    else if (m_Encoding == AudioEncoding::Vorbis)
    {
        audioSerializer.EncodedData.Write(m_ImportEncodedData);
    }
    else
    {
        audioSerializer.Samples.Write(m_ImportSamples);
    }

    m_ImportSamples.clear();
    m_ImportSamples.shrink_to_fit();

    m_ImportEncodedData.clear();
    m_ImportEncodedData.shrink_to_fit();

    audioSerializer.Encoding.Write(m_Encoding);
    audioSerializer.Format.Write(m_Format);
    audioSerializer.SampleRate.Write(m_SampleRate);
    audioSerializer.SampleCount.Write(m_SampleCount);
    audioSerializer.ImportForceMono.Write(m_ImportConfiguration.ForceMono);

    return audioSerializer.Write();
}

bool Pine::AudioFile::DecodeVorbis(const std::vector<std::uint8_t>& encodedData, std::vector<std::int16_t>& samples)
{
    Importer::AudioLoader::AudioData audioData;

    if (!Importer::AudioLoader::DecodeVorbis(encodedData.data(), encodedData.size(), audioData))
    {
        PError(fmt::format("Failed to decode audio asset '{}' as Ogg Vorbis.", m_Path));
        return false;
    }

    // The same fold the importer applies to a clip it stores as PCM, applied here instead because
    // the stored bytes are the source's own.
    if (audioData.Channels > Audio::AudioFormatChannelCount(m_Format))
    {
        Importer::AudioLoader::DownmixToMono(audioData);
    }

    m_Format = audioData.Channels == 2 ? Audio::AudioFormat::Stereo16 : Audio::AudioFormat::Mono16;
    m_SampleRate = audioData.SampleRate;
    m_SampleCount = audioData.SampleCount;

    samples = std::move(audioData.Samples);

    return true;
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
            Audio::Internal::StopPreviewOf(*this);

            Audio::GetAudioAPI()->DestroyBuffer(m_Buffer);
        }

        m_Buffer = nullptr;
    }

    m_ImportSamples.clear();
    m_ImportSamples.shrink_to_fit();

    m_ImportEncodedData.clear();
    m_ImportEncodedData.shrink_to_fit();

    m_State = AssetState::Unloaded;
}

Pine::AudioEncoding Pine::AudioFile::GetEncoding() const
{
    return m_Encoding;
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
