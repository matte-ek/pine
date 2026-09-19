#include "Pine/Assets/AudioFile/Importer/AudioLoader/AudioLoader.hpp"

#include <algorithm>
#include <cstring>

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"

using namespace Pine;

namespace
{
    // The two encodings a 'fmt ' chunk can name. WAVE_FORMAT_EXTENSIBLE files say 0xFFFE here and
    // put the real one in the first two bytes of the extension's SubFormat GUID, which is why the
    // parser unwraps it rather than rejecting it - 24-bit and multichannel files are routinely
    // written that way.
    constexpr std::uint16_t WaveFormatPcm = 0x0001;
    constexpr std::uint16_t WaveFormatFloat = 0x0003;
    constexpr std::uint16_t WaveFormatExtensible = 0xFFFE;

    // The format description a 'fmt ' chunk carries, as far as decoding needs it.
    struct WaveFormat
    {
        std::uint16_t Encoding = 0;
        std::uint16_t Channels = 0;
        std::uint32_t SampleRate = 0;
        std::uint16_t BitsPerSample = 0;
    };

    // Walks a RIFF file without ever reading past its end. Every read is bounds-checked and
    // reports failure rather than trusting a size field in the file, because a chunk header that
    // claims more bytes than the file holds is exactly what a truncated download looks like.
    class ByteReader
    {
    private:
        const std::uint8_t* m_Data;
        std::size_t m_Size;
        std::size_t m_Position = 0;
    public:
        ByteReader(const void* data, const std::size_t size)
            : m_Data(static_cast<const std::uint8_t*>(data)), m_Size(size)
        {
        }

        std::size_t Remaining() const
        {
            return m_Size - m_Position;
        }

        const std::uint8_t* Take(const std::size_t count)
        {
            if (count > Remaining())
            {
                return nullptr;
            }

            const auto position = m_Data + m_Position;

            m_Position += count;

            return position;
        }

        bool Skip(const std::size_t count)
        {
            return Take(count) != nullptr;
        }

        bool ReadUInt16(std::uint16_t& value)
        {
            const auto bytes = Take(sizeof(std::uint16_t));

            if (bytes == nullptr)
            {
                return false;
            }

            memcpy(&value, bytes, sizeof(std::uint16_t));

            return true;
        }

        bool ReadUInt32(std::uint32_t& value)
        {
            const auto bytes = Take(sizeof(std::uint32_t));

            if (bytes == nullptr)
            {
                return false;
            }

            memcpy(&value, bytes, sizeof(std::uint32_t));

            return true;
        }

        // Reads a four character chunk identifier, e.g. "fmt " or "data".
        bool ReadFourCC(char (&value)[4])
        {
            const auto bytes = Take(4);

            if (bytes == nullptr)
            {
                return false;
            }

            memcpy(value, bytes, 4);

            return true;
        }
    };

    bool IsFourCC(const char (&value)[4], const char* expected)
    {
        return memcmp(value, expected, 4) == 0;
    }

    bool ReadFormatChunk(ByteReader& reader, const std::uint32_t chunkSize, WaveFormat& format)
    {
        // The common part of every 'fmt ' chunk: encoding, channels, sample rate, byte rate,
        // block align, bits per sample.
        constexpr std::uint32_t CommonFieldsSize = 16;

        if (chunkSize < CommonFieldsSize)
        {
            PError("Malformed wave file: the format chunk is too short.");
            return false;
        }

        std::uint32_t byteRate;
        std::uint16_t blockAlign;

        if (!reader.ReadUInt16(format.Encoding) ||
            !reader.ReadUInt16(format.Channels) ||
            !reader.ReadUInt32(format.SampleRate) ||
            !reader.ReadUInt32(byteRate) ||
            !reader.ReadUInt16(blockAlign) ||
            !reader.ReadUInt16(format.BitsPerSample))
        {
            PError("Malformed wave file: the format chunk ends past the end of the file.");
            return false;
        }

        auto remainingChunkBytes = chunkSize - CommonFieldsSize;

        // An extensible file's real encoding is the first field of the SubFormat GUID, which sits
        // eight bytes into the extension (after cbSize, valid bits and the channel mask).
        if (format.Encoding == WaveFormatExtensible)
        {
            constexpr std::uint32_t SubFormatOffset = 8;

            if (remainingChunkBytes < SubFormatOffset + sizeof(std::uint16_t))
            {
                PError("Malformed wave file: an extensible format chunk with no sub-format.");
                return false;
            }

            if (!reader.Skip(SubFormatOffset) || !reader.ReadUInt16(format.Encoding))
            {
                return false;
            }

            remainingChunkBytes -= SubFormatOffset + sizeof(std::uint16_t);
        }

        return reader.Skip(remainingChunkBytes);
    }

    // Reads one sample and scales it into the 16-bit range the rest of the engine works in.
    std::int16_t ConvertSample(const std::uint8_t* sample, const WaveFormat& format)
    {
        if (format.Encoding == WaveFormatFloat)
        {
            // Float samples are nominally -1..1, but nothing stops a file from going past that,
            // and wrapping round would turn a loud passage into noise.
            if (format.BitsPerSample == 32)
            {
                float value;
                memcpy(&value, sample, sizeof(float));

                return static_cast<std::int16_t>(std::clamp(value, -1.f, 1.f) * 32767.f);
            }

            double value;
            memcpy(&value, sample, sizeof(double));

            return static_cast<std::int16_t>(std::clamp(value, -1.0, 1.0) * 32767.0);
        }

        switch (format.BitsPerSample)
        {
            case 8:
            {
                // 8-bit wave samples are the odd ones out: unsigned, centred on 128.
                return static_cast<std::int16_t>((static_cast<int>(*sample) - 128) << 8);
            }
            case 16:
            {
                std::int16_t value;
                memcpy(&value, sample, sizeof(std::int16_t));

                return value;
            }
            case 24:
            {
                // Keep the top two bytes; the third is below 16-bit resolution.
                return static_cast<std::int16_t>(sample[1] | (sample[2] << 8));
            }
            default:
            {
                std::int32_t value;
                memcpy(&value, sample, sizeof(std::int32_t));

                return static_cast<std::int16_t>(value >> 16);
            }
        }
    }

    bool IsSupportedFormat(const WaveFormat& format)
    {
        if (format.Channels == 0 || format.SampleRate == 0)
        {
            PError("Unsupported wave file: it declares no channels or no sample rate.");
            return false;
        }

        if (format.Encoding == WaveFormatFloat)
        {
            if (format.BitsPerSample == 32 || format.BitsPerSample == 64)
            {
                return true;
            }

            PError(fmt::format("Unsupported wave file: {}-bit float samples.", format.BitsPerSample));
            return false;
        }

        if (format.Encoding != WaveFormatPcm)
        {
            PError(fmt::format(
                "Unsupported wave file: compressed encoding 0x{:04X}. Only PCM and float are supported.",
                format.Encoding));
            return false;
        }

        if (format.BitsPerSample == 8 || format.BitsPerSample == 16 ||
            format.BitsPerSample == 24 || format.BitsPerSample == 32)
        {
            return true;
        }

        PError(fmt::format("Unsupported wave file: {}-bit PCM samples.", format.BitsPerSample));

        return false;
    }
}

namespace Pine::Importer::AudioLoader
{

bool LoadAudioWave(const std::filesystem::path& file, AudioData& audioData)
{
    const auto fileData = File::ReadRaw(file);

    if (fileData.data == nullptr || fileData.size == 0)
    {
        PError(fmt::format("Failed to read wave file {}", file.string()));
        return false;
    }

    ByteReader reader(fileData.data, fileData.size);

    char riffId[4];
    char formatId[4];
    std::uint32_t riffSize;

    if (!reader.ReadFourCC(riffId) || !reader.ReadUInt32(riffSize) || !reader.ReadFourCC(formatId))
    {
        PError(fmt::format("{} is too short to be a wave file.", file.string()));
        return false;
    }

    if (!IsFourCC(riffId, "RIFF") || !IsFourCC(formatId, "WAVE"))
    {
        PError(fmt::format("{} is not a RIFF/WAVE file.", file.string()));
        return false;
    }

    WaveFormat format;

    auto hasFormat = false;

    // Walk the chunks looking for 'fmt ' and 'data'. Anything else in between - and there usually
    // is something, editors leave 'LIST' and 'fact' chunks behind - is stepped over.
    while (reader.Remaining() >= 8)
    {
        char chunkId[4];
        std::uint32_t chunkSize;

        if (!reader.ReadFourCC(chunkId) || !reader.ReadUInt32(chunkSize))
        {
            break;
        }

        // A chunk's payload is followed by a pad byte when its size is odd. Missing this is what
        // makes a parser lose alignment part way through an otherwise valid file. Widened first,
        // so that a size field of 0xFFFFFFFF does not wrap back round to nothing.
        const std::size_t paddedChunkSize = static_cast<std::size_t>(chunkSize) + (chunkSize % 2);

        if (IsFourCC(chunkId, "fmt "))
        {
            if (!ReadFormatChunk(reader, chunkSize, format))
            {
                return false;
            }

            if (!IsSupportedFormat(format))
            {
                return false;
            }

            // ReadFormatChunk consumes exactly chunkSize bytes, so only the pad is left.
            reader.Skip(paddedChunkSize - chunkSize);

            hasFormat = true;

            continue;
        }

        if (IsFourCC(chunkId, "data"))
        {
            if (!hasFormat)
            {
                PError("Malformed wave file: sample data appears before the format chunk.");
                return false;
            }

            // Trust the file no further than it can be read: a size field claiming more than is
            // actually there is taken as "the rest of the file".
            const auto dataSize = std::min(static_cast<std::size_t>(chunkSize), reader.Remaining());

            const auto sampleData = reader.Take(dataSize);

            if (sampleData == nullptr)
            {
                return false;
            }

            const std::size_t bytesPerSample = format.BitsPerSample / 8;
            const std::size_t bytesPerFrame = bytesPerSample * format.Channels;

            if (bytesPerFrame == 0)
            {
                return false;
            }

            const auto frameCount = dataSize / bytesPerFrame;

            if (frameCount == 0)
            {
                PError(fmt::format("Wave file {} holds no samples.", file.string()));
                return false;
            }

            audioData.Channels = format.Channels;
            audioData.SampleRate = static_cast<int>(format.SampleRate);
            audioData.SampleCount = static_cast<int>(frameCount);
            audioData.Samples.resize(frameCount * format.Channels);

            for (std::size_t sample = 0; sample < audioData.Samples.size(); sample++)
            {
                audioData.Samples[sample] = ConvertSample(sampleData + sample * bytesPerSample, format);
            }

            return true;
        }

        if (!reader.Skip(paddedChunkSize))
        {
            break;
        }
    }

    PError(fmt::format("Wave file {} has no sample data.", file.string()));

    return false;
}

}
