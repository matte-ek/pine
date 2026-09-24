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

    // How a clip's audio is kept in its '.passet'. Serialized as an integer by AudioFile, so only
    // ever append.
    enum class AudioEncoding
    {
        // Interleaved 16-bit PCM, uploaded as it is stored. What a wave source is stored as.
        Pcm = 0,

        // The source's Ogg Vorbis bytes, copied unchanged at import and decoded on every load.
        Vorbis
    };

    inline const char* AudioEncodingToString(const AudioEncoding encoding)
    {
        switch (encoding)
        {
            case AudioEncoding::Pcm:
                return "PCM";
            case AudioEncoding::Vorbis:
                return "Ogg Vorbis";
            default:
                return "Unknown";
        }
    }

    // An audio clip. A wave source is decoded at import and stored as PCM; an Ogg Vorbis source is
    // stored as the Vorbis bytes it came with, several times smaller, and decoded on every load.
    // Either way the whole clip is decoded by the time it has loaded, and lives in one buffer on the
    // audio device, which every source playing this asset shares.
    class AudioFile final : public Asset
    {
    private:
        AudioEncoding m_Encoding = AudioEncoding::Pcm;

        // What the clip plays as, which the import settles on: a Vorbis clip is folded down to
        // this on load, so a force-mono clip keeps its source's stereo bytes but plays in mono.
        Audio::AudioFormat m_Format = Audio::AudioFormat::Mono16;

        int m_SampleRate = 0;

        // Counted per channel, so a mono and a stereo clip of the same length report the same
        // number.
        int m_SampleCount = 0;

        // What the import produced for the '.passet' - PCM samples or Vorbis bytes, depending on
        // m_Encoding - held between Import() and the SaveAssetData() that writes it. Empty at
        // every other time: a loaded asset plays from m_Buffer, and keeping a second copy of a
        // clip in system memory would double what audio costs for nothing.
        std::vector<std::int16_t> m_ImportSamples;
        std::vector<std::uint8_t> m_ImportEncodedData;

        AudioImportConfiguration m_ImportConfiguration;

        Audio::IAudioBuffer* m_Buffer = nullptr;

        struct AudioSerializer : Serialization::Serializer
        {
            // Absent from clips stored before Vorbis was kept, which are all PCM.
            PINE_SERIALIZE_PRIMITIVE(Encoding, Serialization::DataType::Int32);

            PINE_SERIALIZE_PRIMITIVE(Format, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(SampleRate, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(SampleCount, Serialization::DataType::Int32);

            PINE_SERIALIZE_PRIMITIVE(ImportForceMono, Serialization::DataType::Boolean);

            // Only the one m_Encoding names is written.
            PINE_SERIALIZE_ARRAY_FIXED(Samples, std::int16_t);
            PINE_SERIALIZE_ARRAY_FIXED(EncodedData, std::uint8_t);
        };

        // Decodes a stored Vorbis clip into 'samples', folded down to m_Format.
        bool DecodeVorbis(const std::vector<std::uint8_t>& encodedData, std::vector<std::int16_t>& samples);
    protected:
        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;
    public:
        AudioFile();

        AudioEncoding GetEncoding() const;

        Audio::AudioFormat GetFormat() const;
        int GetChannelCount() const;

        int GetSampleRate() const;
        int GetSampleCount() const;

        float GetDuration() const;

        // How many bytes of PCM the clip occupies once decoded, which is what it costs on the audio
        // device. A PCM clip costs roughly the same on disk; a Vorbis clip a fraction of it.
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
