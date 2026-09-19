#pragma once

#include <cstdint>
#include <vector>

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Importer/AssetImporter.hpp"
#include "Pine/Audio/Interfaces/IAudioBuffer.hpp"

namespace Pine
{
    namespace Importer
    {
        class AudioImporter;
    }

    struct AudioImportConfiguration : AssetImportConfiguration
    {
        // Fold a multi-channel source down to a single channel on import. OpenAL only applies
        // panning and distance attenuation to mono buffers, so a stereo clip on a positioned
        // AudioSource plays flat wherever its entity is - this is how a stereo source file is made
        // usable for 3D sound. Off by default, because it is the wrong thing to do to music.
        bool ForceMono = false;
    };

    // A decoded audio clip. The source file's encoding - wave, Ogg Vorbis - exists only at import
    // time: the '.passet' stores interleaved 16-bit PCM, so nothing in the runtime decodes
    // anything. The whole clip lives in one buffer on the audio device, which every source playing
    // this asset shares.
    class AudioFile final : public Asset
    {
    private:
        Audio::AudioFormat m_Format = Audio::AudioFormat::Mono16;

        int m_SampleRate = 0;

        // Counted per channel, so a mono and a stereo clip of the same length report the same
        // number.
        int m_SampleCount = 0;

        // Interleaved 16-bit PCM, held between Import() and the SaveAssetData() that writes it
        // into the '.passet'. Empty at every other time - a loaded asset plays from m_Buffer, and
        // keeping a second copy of a decoded clip in system memory would double what audio costs
        // for nothing.
        std::vector<std::int16_t> m_ImportSamples;

        AudioImportConfiguration m_ImportConfiguration;

        Audio::IAudioBuffer* m_Buffer = nullptr;

        struct AudioSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Format, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(SampleRate, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(SampleCount, Serialization::DataType::Int32);

            PINE_SERIALIZE_PRIMITIVE(ImportForceMono, Serialization::DataType::Boolean);

            PINE_SERIALIZE_ARRAY_FIXED(Samples, std::int16_t);
        };
    protected:
        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;
    public:
        AudioFile();

        Audio::AudioFormat GetFormat() const;
        int GetChannelCount() const;

        int GetSampleRate() const;
        int GetSampleCount() const;

        float GetDuration() const;

        // How many bytes of PCM the clip occupies. Audio is stored decoded, so this is roughly
        // what the asset costs both on disk and on the audio device - which is worth being able
        // to see, since it is the one surprising thing about storing audio this way.
        std::size_t GetSampleDataSize() const;

        AudioImportConfiguration& GetImportConfiguration();

        // The decoded clip on the audio device, or nullptr if the asset has not loaded. Shared by
        // every source playing this file, so it carries no playback state.
        Audio::IAudioBuffer* GetBuffer() const;

        bool Import(Importer::AssetImport* context) override;
        void Dispose() override;

        friend class Importer::AudioImporter;
    };
}
